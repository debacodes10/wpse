#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>
#include "network.h"
#include "protocol.h"
#include <sys/socket.h>
#include <string.h>
#include "cache.h"
#include "pager.h"

#define PORT 8080
#define MAX_EVENTS 64
#define CACHE_SIZE 1024
#define DB_FILE "engine.db"

int main() {
    int server_socket = init_server_socket(PORT);
    if (server_socket < 0) {
        perror("Failed to initialize server socket");
        exit(EXIT_FAILURE);
    }

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1 || add_to_epoll(epoll_fd, server_socket, EPOLLIN) == -1) {
        perror("Failed to setup epoll framework");
        exit(EXIT_FAILURE);
    }

    cache_t *engine_cache = cache_create(CACHE_SIZE);
    if (!engine_cache){
      perror("Failed to allocate memory cache.\n");
      exit(EXIT_FAILURE);
    }

    pager_t *db_pager = pager_open(DB_FILE);
    if (!db_pager){
      perror("Failed to initialize persistent block storage engine.");
      exit(EXIT_FAILURE);
    }

    uint8_t static_disk_page[PAGE_SIZE];
    memset(static_disk_page, 0, PAGE_SIZE);

    if (db_pager->total_pages > 0){
      if (pager_read_page(db_pager, 0, static_disk_page) == 0){
        if (static_disk_page[0] == 'D' && static_disk_page[1] == 'B'){
          uint8_t val_len = static_disk_page[2];
          char saved_key[] = "user_id";

          char recovered_val[256];
          memcpy(recovered_val, &static_disk_page[3], val_len);
          recovered_val[val_len] = '\0';

          cache_set(engine_cache, saved_key, (uint8_t*)recovered_val, val_len);
          printf("RECOVERY: Found existing storage page. Restored key [user_id] = [%s] from disk!\n", recovered_val);
        }
      }
    }

    struct epoll_event events[MAX_EVENTS];
    printf("Modular storage engine with disk persistence running on port %d...\n", PORT);

    while (1) {
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (num_events == -1) {
            if (errno == EINTR) continue;
            break;
        }

        for (int i = 0; i < num_events; i++) {
            if (events[i].data.fd == server_socket) {
                // Handle new incoming client connections
                while (1) {
                    int client_socket = accept(server_socket, NULL, NULL);
                    if (client_socket < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        perror("Accept error");
                        break;
                    }
                    
                    if (make_socket_non_blocking(client_socket) < 0 ||
                        add_to_epoll(epoll_fd, client_socket, EPOLLIN | EPOLLET) == -1) {
                        close(client_socket);
                    }
                }
            } else {
                // Process data from an existing client connection
                int client_fd = events[i].data.fd;
                db_header_t header;
                
                // Read exact header dimensions
                ssize_t bytes_read = read_exact(client_fd, &header, sizeof(db_header_t));
                if (bytes_read <= 0) {
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                    close(client_fd);
                    continue;
                }

                if (parse_header(&header) < 0) {
                    char err_msg[] = "Error: Invalid Wire Protocol Header\n";
                    send(client_fd, err_msg, sizeof(err_msg), 0);
                    continue;
                }

                printf("Valid Command Received: Opcode=0x%x, KeyLen=%d, ValLen=%d\n", 
                       header.opcode, header.key_len, header.val_len);
                
                char *key = malloc(header.key_len+1);
                uint8_t *value = malloc(header.val_len+1);

                if(header.key_len > 0){
                  read_exact(client_fd, key, header.key_len);
                  key[header.key_len] = '\0';
                } else {
                  key[0] = '\0';
                }

                if (header.val_len>0){
                  read_exact(client_fd, value, header.val_len);
                  value[header.val_len] = '\0';
                }

                if(header.opcode == OP_SET){
                  cache_set(engine_cache, key, value, header.val_len);
                  printf("ENGINE: SET Key=[%s] ValLen=%d\n", key, header.val_len);
                  
                if (strcmp(key, "user_id") == 0 && header.val_len < 250){
                  memset(static_disk_page, 0, PAGE_SIZE);
                  static_disk_page[0] = 'D';
                  static_disk_page[1] = 'B';
                  static_disk_page[2] = header.val_len;
                  memcpy(&static_disk_page[3], value, header.val_len);

                  pager_write_page(db_pager, 0, static_disk_page);
                  pager_sync(db_pager);
                  printf("PERSISTENCE: Synchronized key [%s] safely down to Page 0 blocks.\n", key);
                }

                  char ack[] = "STORED\n";
                  send(client_fd, ack, sizeof(ack) - 1, 0);

                } else if (header.opcode == OP_GET){
                  cache_node_t *node = cache_get(engine_cache, key);
                  if (node) {
                      printf("ENGINE: HIT Key=[%s]\n", key);
                      // Send back the actual binary value data from memory
                      send(client_fd, node->value, node->val_len, 0);
                      send(client_fd, "\n", 1, 0);
                  } else {
                      printf("ENGINE: MISS Key=[%s]\n", key);
                      char not_found[] = "ERR: NOT_FOUND\n";
                      send(client_fd, not_found, sizeof(not_found) - 1, 0);
                  }
                } else {
                  char unknown[] = "ERR: UNKNOWN_OPCODE\n";
                  send(client_fd, unknown, sizeof(unknown) - 1, 0);
                }

                printf("Successfully parsed payload -> Key: [%s] | ValLen: %d\n", key, header.val_len);
                
                // Clean up allocations for this iteration
                free(key);
                free(value);
            }
        }
    }

    cache_free(engine_cache);
    pager_close(db_pager);
    close(server_socket);
    close(epoll_fd);
    return 0;
}
