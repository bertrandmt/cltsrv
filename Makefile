CFLAGS += -g

vpath %.o client server
vpath %.h client server
vpath %.c client server

fork: fork.o server.o client.o
fork.o: fork.c client.h server.h
client.o: client.c client.h
server.o: server.c server.h
