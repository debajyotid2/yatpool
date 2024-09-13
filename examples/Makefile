CC=g++
CFLAGS=-g
LDFLAGS=-lyatpool
LIB=-L../build/src
INCLUDE=-I../src
SRC=$(wildcard *.c)
BIN_DIR=bin
BIN=$(patsubst %.c, $(BIN_DIR)/%, $(SRC))

all: $(BIN_DIR) $(BIN)

$(BIN_DIR):
	mkdir $(BIN_DIR)

$(BIN_DIR)/%: %.c
	$(CC) $(CFLAGS) $(INCLUDE) $(LIB) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf $(BIN_DIR)/*
