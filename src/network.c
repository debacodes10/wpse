#include "network.h"
#include <stdio.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <netinet/in.h>

int make_socket_non_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int init_server_socket(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) return -1;

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) return -1;
    if (make_socket_non_blocking(server_fd) < 0) return -1;
    if (listen(server_fd, SOMAXCONN) < 0) return -1;

    return server_fd;
}

int add_to_epoll(int epoll_fd, int fd, uint32_t events) {
    struct epoll_event ev = { .events = events, .data.fd = fd };
    return epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
}
