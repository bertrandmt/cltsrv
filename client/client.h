#ifndef CLIENT_H_
#define CLIENT_H_

#include <stdint.h>

struct client {
    int sockfd;
    uint8_t id;
    int success;
};

int client_setup(struct client *c);
int client_main_loop(struct client *c);

#endif /* CLIENT_H_ */
