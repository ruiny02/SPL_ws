CC = gcc
TARGET_SERVER := w12_server
TARGET_CLIENT := w12_client
CFLAGS = -Wall -Wextra -Werror -std=gnu2x

all: $(TARGET_SERVER) $(TARGET_CLIENT)

.PHONY: clean
clean:
	rm -f $(TARGET_SERVER) $(TARGET_CLIENT) $(TARGET_SERVER).o $(TARGET_CLIENT).o
