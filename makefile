CC=gcc
CFLAGS=-lev
all:
	$(CC) $(CFLAGS) src/main.c src/pylon.c src/servercheck.c src/valuelist.c src/daemon.c -o pylon
