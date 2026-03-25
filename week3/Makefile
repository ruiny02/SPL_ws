CC = gcc
CFLAGS = -ggdb -c -O0
LDFLAGS = -ggdb -O0

w3: main.o minus.o plus.o
	$(CC) $(LDFLAGS) -o $@ $^

main.o: main.c
	$(CC) $(CFLAGS) $<

minus.o: minus.c
	$(CC) $(CFLAGS) $<

plus.o: plus.c
	$(CC) $(CFLAGS) $<

clean:
	rm -f *.o w3
