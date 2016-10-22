#ifndef SERVER_H_
#define SERVER_H_

#include <sys/select.h>

struct server {
    int sockfd;
    int success;
};

int server_setup(struct server *s);
int server_main_loop(struct server *s);

#endif /* SERVER_H_ */
