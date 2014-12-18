CC=gcc
CFLAGS=-lev
SRCDIR=src
FILES=$(SRCDIR)/main.c $(SRCDIR)/pylon.c $(SRCDIR)/servergraph.c $(SRCDIR)/valuelist.c $(SRCDIR)/daemon.c
OUTPUT=pylon

all:
ifeq ($(shell ls /usr/include/libev/ev.h 2>/dev/null),)
	$(CC) $(CFLAGS) $(FILES) -o $(OUTPUT)
else
	$(CC) $(CFLAGS) -D_EVSUB $(FILES) -o $(OUTPUT)
endif
