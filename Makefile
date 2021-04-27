# Makefile
TARGETS = dbServer dbEditor atm interest

CC_C = $(CROSS_TOOL)gcc

CFLAGS = -D_POSIX_SOURCE -Wall -g -std=c99 -Werror

all: clean $(TARGETS)

$(TARGETS):
	$(CC_C) $(CFLAGS) $@.c -o $@

clean:
	rm -f $(TARGETS)
