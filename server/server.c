#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    sig_t sig = signal(SIGPIPE, sigpipe_handler);
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

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(s->sockfd, &readfds);
    int nfds = s->sockfd + 1;

    while (1) {
        fd_set readfds_cpy;
        FD_COPY(&readfds, &readfds_cpy);

        int rc = select(nfds, &readfds_cpy, NULL, NULL, NULL);
        if (rc < 0) {
            perror("failed to select for sockets");
            goto exit_failure;
        }

        /* process ready fds */
        for (int fd = 0; fd < nfds; fd++) {
            if (FD_ISSET(fd, &readfds_cpy)) {
                if (fd == s->sockfd) {
                    /* this is the bound socket: accept new connection */
                    struct sockaddr_un cli_addr;
                    socklen_t clilen = sizeof cli_addr;
                    int new_fd = accept(s->sockfd, (struct sockaddr *)&cli_addr, &clilen);
                    if (new_fd < 0) {
                        perror("failed to accept from Unix socket");
                        goto exit_failure;
                    }
                    printf("accepted %d\n", new_fd);
                    FD_SET(new_fd, &readfds);
                    if (new_fd >= nfds) nfds = new_fd + 1;
                }
                else {
                    /* this is any other socket: read and ack */
                    char msg[256] = { 0 };
                    ssize_t nread = read(fd, msg, sizeof msg - 1);
                    if (nread < 0) {
                        perror("failed to read from socket");
                        FD_CLR(fd, &readfds);
                        close(fd);
                        continue;
                    }
                    printf("received: \"%s\"\n", msg);

                    ssize_t nwritten = write(fd, "ACK", 4);
                    if (nwritten < 0) {
                        perror("failed to write to socket");
                        FD_CLR(fd, &readfds);
                        close(fd);
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
