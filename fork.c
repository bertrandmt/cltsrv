#include "client/client.h"
#include "server/server.h"

#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    struct server s;
    struct client c;

    server_setup(&s);
    client_setup(&c);
    if (!s.success || !c.success) {
        return EXIT_FAILURE;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("failed to fork");
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        /* child */
        return client_main_loop(&c);
    }
    else {
        /* child */
        printf("started client subprocess with pid %d\n", (int)pid);
        return server_main_loop(&s);
    }
}
