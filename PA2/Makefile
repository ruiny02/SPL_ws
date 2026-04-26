CC=gcc
# CFLAGS=-Wall -Wextra -Werror -ggdb -std=gnu2x
# C standard does not matter, just has to be gnu...
CFLAGS=-Wall -Wextra -ggdb -std=gnu2x
LDFLAGS=-ledit

BIN_DIR=bin
BUILD_DIR=build

# note: object files can be placed anywhere, just an example
EXEC_SRCS=$(wildcard executables_src/*.c)
EXEC_OBJS=$(patsubst executables_src/%.c,$(BUILD_DIR)/%.o,$(EXEC_SRCS))
EXEC_TARGETS=$(addprefix $(BIN_DIR)/, $(notdir $(EXEC_SRCS:.c=)))
SHELL_SRCS=$(wildcard shell_src/*.c)
SHELL_OBJS=$(patsubst shell_src/%.c,$(BUILD_DIR)/%.o,$(SHELL_SRCS))

SHELL_TARGET=$(BIN_DIR)/pa2


all: create_dirs $(SHELL_TARGET) $(EXEC_TARGETS)

$(SHELL_TARGET): $(SHELL_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(SHELL_OBJS): $(BUILD_DIR)/%.o: shell_src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(EXEC_TARGETS): $(BIN_DIR)/%: $(BUILD_DIR)/%.o
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(BUILD_DIR)/%.o: executables_src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean create_dirs

create_dirs:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BIN_DIR)

clean:
	rm -rf $(BIN_DIR) $(BUILD_DIR)
