#include "client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>
#include <strings.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/un.h>

#define DEV_URANDOM "/dev/urandom"
#define SOCKET      "socket"

static int get_random_byte(uint8_t *pb) {
    int fd = open(DEV_URANDOM, O_RDONLY);
    if (fd < 0) {
        perror("failed to open " DEV_URANDOM);
        return fd;
    }

    ssize_t nread = read(fd, pb, sizeof *pb);
    if (nread < 0) {
        perror("failed to read from " DEV_URANDOM);
        return nread;
    }

    int rc = close(fd);
    if (rc < 0) {
        perror("failed to close " DEV_URANDOM);
        return rc;
    }

    return 0;
}

int client_setup(struct client *c) {
    if (NULL == c) return EXIT_FAILURE;

    c->sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (c->sockfd < 0) {
        perror("failed to open Unix socket");
        goto exit_failure;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof addr);
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET, sizeof addr.sun_path - 1);
    int rc = connect(c->sockfd, (struct sockaddr *)&addr, sizeof addr);
    if (rc < 0) {
        perror("failed to connect to Unix socket");
        goto exit_failure;
    }

    rc = get_random_byte(&c->id);
    if (rc < 0) {
        goto exit_failure;
    }

    c->success = 1;
    return EXIT_SUCCESS;

exit_failure:
    c->success = 0;
    return EXIT_FAILURE;
}

int client_main_loop(struct client *c) {
    if (NULL == c) return EXIT_FAILURE;
    if (!c->success) return EXIT_FAILURE;

    while (1) {
        char msg[256];
        snprintf(msg, sizeof msg, "recurrent message from client #%d", c->id);

        ssize_t nwritten = write(c->sockfd, msg, strlen(msg));
        if (nwritten < 0) {
            perror("failed to write to Unix socket");
            goto exit_failure;
        }

        bzero(msg, sizeof msg);
        ssize_t nread = read(c->sockfd, msg, sizeof msg - 1);
        if (nread < 0) {
            perror("failed to read from Unix socket");
            goto exit_failure;
        }

        uint8_t sleep_amount = 2;
        get_random_byte(&sleep_amount);
        sleep(sleep_amount >> 5);
    }

    return EXIT_SUCCESS;

exit_failure:
    c->success = 0;
    return EXIT_FAILURE;
}
