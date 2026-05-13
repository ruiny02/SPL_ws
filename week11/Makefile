CC=gcc
CFLAGS=-Wall -Wextra -ggdb -std=gnu2x -pthread
TARGET=w11
TARGETS=$(TARGET) $(TARGET)_debug

all: $(TARGET) $(TARGET)_debug

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) $< -o $@

$(TARGET)_debug.o: $(TARGET).c
	$(CC) $(CFLAGS) -DDEBUG -c $< -o $@

$(TARGET)_debug: $(TARGET)_debug.o
	$(CC) $(CFLAGS) $< -o $@

.PHONY: all clean test

clean:
	rm -f $(TARGETS) *.o

test:
	@echo "Running valgrind on $(TARGET)..."
	@valgrind --leak-check=full --track-origins=yes --show-leak-kinds=all ./$(TARGET) 10 10
	@echo "Valgrind finished."
