CC=gcc
CFLAGS=-Wall -Wextra -Iinclude -MMD -MP
LIBS= -lcurl -lcjson -lcrypto
TARGET=build/gitool

SRCS:= $(wildcard src/*.c)
OBJS:= $(patsubst src/%.c,build/%.o,$(SRCS))
DEPS:= $(SRCS:.c=.d)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LIBS)

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) -c $< -o $@ $(CFLAGS)

-include $(DEPS)

clean:
	rm -rf build

install:
	install -Dm755 $(TARGET) $(DESTDIR)/usr/local/bin/gitool
	install -Dm644 doc/gitool.1 $(DESTDIR)/usr/local/share/man/man1/gitool.1
	mandb

uninstall:
	rm -f $(DESTDIR)/usr/local/bin/gitool
	rm -f $(DESTDIR)/usr/local/share/man/man1/gitool.1
	mandb

.PHONY: all clean install uninstall
