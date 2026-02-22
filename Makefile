CC ?= clang-19
PKGS ?= libcurl ncurses readline
CFLAGS += -O2 -g -Wall -Wextra $(shell pkg-config --cflags $(PKGS))
LDLIBS += $(shell pkg-config --libs $(PKGS))

xi: xi.c
	$(CC) $(CFLAGS) $< mjson.o -o $@ $(LDLIBS)

.PHONY: clean
clean:
	rm -rf xi
