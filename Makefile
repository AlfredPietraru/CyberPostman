CC = gcc
CFLAGS = -Wall -Wextra
LDFLAGS = -lm

build: server subscriber 

server: server.c
	$(CC) -g -o $@ $^ $(CCFLAGS) $(LDFLAGS)

subscriber: subscriber.c
	$(CC) -g -o $@ $^ $(CCFLAGS) $(LDFLAGS)

clean:
	rm -f server subscriber