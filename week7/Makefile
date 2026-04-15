CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=gnu2x
LDFLAGS = -ledit

SRC = w7.c
OBJ = $(SRC:.c=.o)
TARGET = w7

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(OBJ) $(TARGET)
