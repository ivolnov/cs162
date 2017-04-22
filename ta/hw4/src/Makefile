CFLAGS = -std=gnu99 -ggdb3 -Wall
SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o) index.o

all: $(OBJS)

%.o: %.c
	gcc -c $(CFLAGS) $< -o $@

%.o: %.S
	gcc -c $(CFLAGS) $< -o $@

clean:
	rm -rf -- *.o
