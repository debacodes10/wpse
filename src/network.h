#ifndef NETWORK_H
#define NETWORK_H

#include <sys/types.h>
#include <stdint.h>

int make_socket_non_blocking(int fd);
int init_server_socket(int port);
int add_to_epoll(int epoll_fd, int fd, uint32_t events);
ssize_t read_exact(int fd, void *buf, size_t count);

#endif
