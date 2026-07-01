#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>
#include "network.h"
#include "protocol.h"
#include <sys/socket.h>

#define PORT 8080
#define MAX_EVENTS 64

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

    struct epoll_event events[MAX_EVENTS];
    printf("Modular storage engine running on port %d...\n", PORT);

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
                }

                printf("Successfully parsed payload -> Key: [%s] | ValLen: %d\n", key, header.val_len);
                
                // Response back to client
                char ack[] = "Command Pack Received & Parsed Successfully\n";
                send(client_fd, ack, sizeof(ack), 0);

                // Clean up allocations for this iteration
                free(key);
                free(value);
            }
        }
    }

    close(server_socket);
    close(epoll_fd);
    return 0;
}
