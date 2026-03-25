CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11
TARGET = w4

.PHONY: all clean test

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c

clean:
	rm -f $(TARGET) *.no
