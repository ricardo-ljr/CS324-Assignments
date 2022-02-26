CC = gcc
CFLAGS = -g

all: client server client-udp server-udp

fork: client.c
	$(CC) $(CFLAGS) -o client client.c
	

exec: server.c
	$(CC) $(CFLAGS) -o server server.c

fork2: client-udp.c
	$(CC) $(CFLAGS) -o client-udp client-udp.c

exec2: server-udp.c
	$(CC) $(CFLAGS) -o server-udp server-udp.c