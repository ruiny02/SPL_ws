CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=gnu11
TARGET = w9
SRC = w9.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
