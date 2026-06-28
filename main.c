#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_EVENTS 64


int make_socket_non_blocking(int fd){
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1){
    perror("fntl F_GETFL");
    return -1;
  }
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1){
    perror("fcntl F_SETFL O_NONBLOCK");
    return -1;
  }
  return 0;
}


int main(){

  int server_socket = socket(AF_INET, SOCK_STREAM, 0);
  if(server_socket < 0){
    perror("Socket creation failed.");
    exit(EXIT_FAILURE);
  }

  int opt = 1;
  setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in server_address;
  server_address.sin_family = AF_INET;
  server_address.sin_port = htons(8080);
  server_address.sin_addr.s_addr = INADDR_ANY;

  if (bind(server_socket, (struct sockaddr*) &server_address, sizeof(server_address)) < 0){
    perror("Binding to port failed.");
    exit(EXIT_FAILURE);
  }

  if (make_socket_non_blocking(server_socket)<0){
    exit(EXIT_FAILURE);
  }

  if (listen(server_socket, SOMAXCONN)< 0){
    perror("Listening for connection failed.");
    exit(EXIT_FAILURE);
  }

  // Create epoll instance
  int epoll_fd = epoll_create1(0);
  if (epoll_fd<0){
    perror("epoll_create1 failed,");
    exit(EXIT_FAILURE);
  }

  // Register listening server socket to epoll interest list
  struct epoll_event event;
  event.events = EPOLLIN;
  event.data.fd = server_socket;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &event) == -1){
    perror("epoll_ctl server_socket failed");
    exit(EXIT_FAILURE);
  }

  struct epoll_event events[MAX_EVENTS];
  printf("Storage engine event loop running on port 8080...\n");

  // Epoll event loop
  while (1){
    int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
    if (num_events == -1){
      if (errno == EINTR) {
        continue;
      }
      perror("epoll_wait failed");
      break;
    }

    for (int i = 0;i<num_events;i++){
      if (events[i].data.fd == server_socket){
        while (1) {
          int client_socket = accept(server_socket, NULL, NULL);
          if (client_socket < 0){
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
              break;
            }
            perror("accept failed");
            break;
          }
          if (make_socket_non_blocking(client_socket)<0){
            close(client_socket);
            continue;
          }
          struct epoll_event client_ev;
          client_ev.events = EPOLLIN;
          client_ev.data.fd = client_socket;
          if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_socket, &client_ev) == -1){
            perror("epoll_ctl client adding failed.");
            close(client_socket);
          }
          printf("New client connected on socket fd: %d\n", client_socket);
        }
      } else {
        int client_fd = events[i].data.fd;
        char read_buffer[512];
        ssize_t bytes_read = read(client_fd, read_buffer, sizeof(read_buffer)-1);
        if (bytes_read <= 0){
          if (bytes_read<=0 && (errno==EAGAIN || errno==EWOULDBLOCK)){
            continue;
          }
          printf("Client on fd %d disconnected.\n", client_fd);
          epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
          close(client_fd);
        } else {
          read_buffer[bytes_read] = '\0';
          printf("Received payload from fd %d: %s.\n", client_fd, read_buffer);
          char reply[] = "Payload received by engine,\n";
          send(client_fd, reply, sizeof(reply), 0);
        }
      }
    }

  }

  /* int client_socket;
  client_socket = accept(server_socket, NULL, NULL);
  if (client_socket < 0){
    perror("Client socket negative, could not connect.");
    exit(EXIT_FAILURE);
  }

  send(client_socket, message, sizeof(message), 0); */

  close(server_socket);
  close(epoll_fd);

  return 0;
}
