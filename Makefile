CC ?= cc
CFLAGS ?= -std=c17 -O2 -Wall -Wextra -Wpedantic -Werror

all: build/integrity

build/integrity: src/main.c
	mkdir -p build
	$(CC) $(CFLAGS) $< -o $@ -lcrypto

clean:
	rm -rf build

.PHONY: all clean
