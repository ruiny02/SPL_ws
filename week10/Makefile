CC = gcc
TARGET_SERVER := server
TARGET_CLIENT := client
CCFLAGS = -Wall -Wextra -Werror

all: $(TARGET_SERVER) $(TARGET_CLIENT)

$(TARGET_SERVER): $(TARGET_SERVER).o
	$(CC) $(CCFLAGS) $< -o $@

$(TARGET_CLIENT): $(TARGET_CLIENT).o
	$(CC) $(CCFLAGS) $< -o $@ -lreadline

$(TARGET_SERVER).o: $(TARGET_SERVER).c
	$(CC) $(CCFLAGS) -c $< -o $@

$(TARGET_CLIENT).o: $(TARGET_CLIENT).c
	$(CC) $(CCFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(TARGET_SERVER) $(TARGET_CLIENT) $(TARGET_SERVER).o $(TARGET_CLIENT).o
