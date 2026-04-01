CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=gnu2x
LDFLAGS = -ledit

SRC = week5.c
OBJ = $(SRC:.c=.o)
TARGET = w5

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(OBJ) $(TARGET) 
