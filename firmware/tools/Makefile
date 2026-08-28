CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2

.PHONY: all clean

all: teletekst-replay

teletekst-replay: teletekst_replay.c ../src/teletekst.c ../src/teletekst.h
	$(CC) $(CFLAGS) -I../src teletekst_replay.c ../src/teletekst.c -o $@

clean:
	rm -f teletekst-replay
