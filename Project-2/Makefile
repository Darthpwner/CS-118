CC=gcc

all: server client

server: server.c
	$(CC) server.c -o server -w

client: client.c
	$(CC) client.c -o client -w

.PHONY: clean
	
clean:
	rm client server