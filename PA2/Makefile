CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=gnu11
CPPFLAGS = -MMD -MP

BUILD_DIR = build
BIN_DIR = bin
SRC_DIR = executable_src

COMMON_OBJ = $(BUILD_DIR)/common.o
BINARIES = \
	$(BIN_DIR)/pa2 \
	$(BIN_DIR)/pa2_head \
	$(BIN_DIR)/pa2_tail \
	$(BIN_DIR)/pa2_cat \
	$(BIN_DIR)/pa2_cp \
	$(BIN_DIR)/pa2_mv \
	$(BIN_DIR)/pa2_rm

.PHONY: all clean

all: $(BINARIES)

$(BUILD_DIR) $(BIN_DIR):
	mkdir -p $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -I$(SRC_DIR) -c $< -o $@

$(BIN_DIR)/pa2_head: $(BUILD_DIR)/pa2_head.o $(COMMON_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/pa2.o: shell_src/pa2.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BIN_DIR)/pa2: $(BUILD_DIR)/pa2.o | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@ -ledit

$(BIN_DIR)/pa2_tail: $(BUILD_DIR)/pa2_tail.o $(COMMON_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(BIN_DIR)/pa2_cat: $(BUILD_DIR)/pa2_cat.o $(COMMON_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(BIN_DIR)/pa2_cp: $(BUILD_DIR)/pa2_cp.o $(COMMON_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(BIN_DIR)/pa2_mv: $(BUILD_DIR)/pa2_mv.o $(COMMON_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(BIN_DIR)/pa2_rm: $(BUILD_DIR)/pa2_rm.o $(COMMON_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

-include $(BUILD_DIR)/*.d
