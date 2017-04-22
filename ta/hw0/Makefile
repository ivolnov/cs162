CC=gcc
CFLAGS=-Wall -g
EXECUTABLE=app

.PHONY: clean wc main map

wc: wc.c
	$(CC) $(CFLAGS) $^ -o $@

main: main.c
	$(CC) $(CFLAGS) $^ -o $@

map: map.c
	$(CC) $(CFLAGS) $^ -o $@

clean:
	/bin/rm -f wc map main
