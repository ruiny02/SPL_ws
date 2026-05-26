SERVER_SRCS := $(wildcard server/*.c)
SERVER_OBJS := $(SERVER_SRCS:.c=.o)
ARGON2_LDLIBS := $(shell if ldconfig -p 2>/dev/null | grep -q 'libargon2\.so '; then printf '%s' '-largon2'; elif ldconfig -p 2>/dev/null | grep -q 'libargon2\.so\.1'; then printf '%s' '-l:libargon2.so.1'; else printf '%s' '-largon2'; fi)

pa3_server: LDFLAGS += $(ARGON2_LDLIBS)
pa3_server: $(SERVER_OBJS) $(COMMON_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean_pa3_server:
	rm -f $(SERVER_OBJS) pa3_server
