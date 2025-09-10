CC = gcc
DEBUG_FLAGS = -O0 -g -DDEBUG -DERROR_LOGGING -DWARNING_LOGGING
PERFORMANCE_FLAGS = -O3
CFLAGS = -Wall -Wextra -fshort-enums -Iinclude
LIBS = -lmpfr -lgmp

SRC = $(wildcard src/*.c)
BIN = bin/app

.PHONY: all debug performance clean

all: performance

debug: $(BIN)_debug

$(BIN)_debug: $(SRC)
	$(CC) $(CFLAGS) $(DEBUG_FLAGS) -o $@ $^ $(LIBS)

performance: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(PERFORMANCE_FLAGS) -o $@ $^ $(LIBS)

clean:
	rm -f $(BIN) $(BIN)_debug
