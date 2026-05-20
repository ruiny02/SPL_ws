CC=gcc
CFLAGS=-Wall -Wextra -Wpedantic -Werror -ggdb -std=gnu2x -D_GNU_SOURCE -Iinclude

all: pa3_server pa3_client

include common/common.mk
include server/server.mk
include client/client.mk

.PHONY: clean create_dirs clean_common clean_pa3_server clean_pa3_client

clean: clean_common clean_pa3_server clean_pa3_client
