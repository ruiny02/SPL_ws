CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=gnu2x -pthread
TARGET = w13

all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) $< -o $@

.PHONY: all clean

clean:
	rm -f $(TARGET) *.o
