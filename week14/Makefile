CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=gnu2x -pthread
TARGET = 2021312344-w14
SRC = $(TARGET).c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $< -o $@

.PHONY: all clean

clean:
	rm -f $(TARGET) *.o
