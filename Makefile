CC ?= gcc
PKGS ?= libcurl ncurses
CFLAGS += -O2 -Wall -Wextra $(shell pkg-config --cflags $(PKGS))
LDLIBS += $(shell pkg-config --libs $(PKGS))

xi: xi.c
	$(CC) $(CFLAGS) $< mjson.o -o $@ $(LDLIBS)

.PHONY: clean
clean:
	rm -rf xi
