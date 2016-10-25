#include "server.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <poll.h>
#include <signal.h>
#include <unistd.h>

#include <sys/errno.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET  "socket"

static void sigpipe_handler(int signal) {
    (void) signal;
    if (0 != errno) perror("sigpipe_handler");
}

int server_setup(struct server *s) {
    if (NULL == s) return EXIT_FAILURE;

    void (*sig)(int) = signal(SIGPIPE, sigpipe_handler);
    if (SIG_ERR == sig) {
        perror("failed to install SIGPIPE handler");
        return EXIT_FAILURE;
    }

    s->sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s->sockfd < 0) {
        perror("failed to open Unix socket");
        goto exit_failure;
    }

    /* unlink before bind */
    int rc = unlink(SOCKET);
    if (rc < 0 && errno != ENOENT) {
        perror("failed to unlink socket");
        goto exit_failure;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET, sizeof addr.sun_path - 1);
    rc = bind(s->sockfd, (struct sockaddr *)&addr, sizeof addr);
    if (rc < 0) {
        perror("failed to bind to Unix socket");
        goto exit_failure;
    }

    rc = listen(s->sockfd, 5);
    if (rc < 0) {
        perror("failed to listen to Unix socket");
        goto exit_failure;
    }

    s->success = 1;
    return EXIT_SUCCESS;

exit_failure:
    s->success = 0;
    return EXIT_FAILURE;
}

int server_main_loop(struct server *s) {
    if (NULL == s) return EXIT_FAILURE;
    if (!s->success) return EXIT_FAILURE;

#define MAX_NFDS    (100)
    struct pollfd fds[MAX_NFDS] = { { 0 } };
    nfds_t nfds = 0;

    /* set socket fd */
    fds[nfds].fd = s->sockfd;
    fds[nfds].events = POLLIN;
    nfds++;

    while (1) {
        int rc = poll(fds, nfds, -1);
        if (rc < 0) {
            perror("failed to select for sockets");
            goto exit_failure;
        }
        assert(0 != rc);

        /* process ready fds */
        for (nfds_t i = 0; i < nfds; i++) {
            if (0 != fds[i].revents) {
                if (fds[i].fd == s->sockfd) {
                    assert(POLLIN == fds[i].revents);

                    /* this is the bound socket: accept new connection */
                    struct sockaddr_un cli_addr;
                    socklen_t clilen = sizeof cli_addr;
                    int new_fd = accept(s->sockfd, (struct sockaddr *)&cli_addr, &clilen);
                    if (new_fd < 0) {
                        perror("failed to accept from Unix socket");
                        goto exit_failure;
                    }
                    printf("accepted %d\n", new_fd);

                    assert(nfds != MAX_NFDS);
                    fds[nfds].fd = new_fd;
                    fds[nfds].events = POLLIN;
                    nfds++;
                }
                else {
                    assert(POLLIN == fds[i].revents);

                    /* this is any other socket: read and ack */
                    char msg[256] = { 0 };
                    ssize_t nread = read(fds[i].fd, msg, sizeof msg - 1);
                    if (nread < 0) {
                        perror("failed to read from socket");
                        fds[i].events = 0;
                        close(fds[i].fd);
                        continue;
                    }
                    printf("received: \"%s\"\n", msg);

                    ssize_t nwritten = write(fds[i].fd, "ACK", 4);
                    if (nwritten < 0) {
                        perror("failed to write to socket");
                        fds[i].events = 0;
                        close(fds[i].fd);
                        continue;
                    }
                }
            }
        }
    }

    return EXIT_SUCCESS;

exit_failure:
    s->success = 0;
    return EXIT_FAILURE;
}
