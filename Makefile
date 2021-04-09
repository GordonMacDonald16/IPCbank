# Makefile
TARGETS = dbServer dbEditor

CC_C = $(CROSS_TOOL)gcc

CFLAGS = -Wall -g -std=c99 -Werror

all: clean $(TARGETS)

$(TARGETS):
	$(CC_C) $(CFLAGS) $@.c -o $@

clean:
	rm -f $(TARGETS)