CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11
TARGET = w6
SRC = w6.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
