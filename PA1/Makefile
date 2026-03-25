CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Werror -O2

OBJS = main.o util.o io.o index.o query.o

pa1: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

main.o: main.c pa1.h
	$(CC) $(CFLAGS) -c main.c

util.o: util.c pa1.h
	$(CC) $(CFLAGS) -c util.c

io.o: io.c pa1.h
	$(CC) $(CFLAGS) -c io.c

index.o: index.c pa1.h
	$(CC) $(CFLAGS) -c index.c

query.o: query.c pa1.h
	$(CC) $(CFLAGS) -c query.c

clean:
	rm -f $(OBJS) pa1
