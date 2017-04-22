
SRC = src
MAIN_SRC = $(SRC)/main
CFLAGS = -std=gnu99 -ggdb3 -Wall -I $(SRC)
MKDIR_P = mkdir -p

LINKFLAGS = -lpthread
BIN = ./bin

SRCS = $(wildcard $(SRC)/*.c)
MAIN_SRCS = $(wildcard $(MAIN_SRC)/*.c)

OBJS = $(SRCS:.c=.o) $(SRC)/index.o
MAIN_OBJS = $(MAIN_SRCS:.c=.o)

all: $(BIN)/kvfollower $(BIN)/tpcleader
	ln -sf ../$(MAIN_SRC)/kvsystem bin/kvsystem

$(BIN)/%: $(MAIN_SRC)/%.o $(OBJS)
	$(MKDIR_P) $(BIN)
	gcc $(OBJS) $< $(LINKFLAGS) -o $@

%.o: %.c
	gcc -c $(CFLAGS) $< -o $@

%.o: %.S
	gcc -c $(CFLAGS) $< -o $@

$(SRC)/index.o: $(SRC)/index.h $(SRC)/index.S $(SRC)/index.html

skeleton:
	./create_skeleton

clean:
	rm -rf $(BIN)
	$(MAKE) -C $(SRC) clean

.PHONY: all clean
