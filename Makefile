# Raindrop Chimes. Needs a C99 compiler and the maths library, nothing else.
#
#   make          build ./chimes
#   make test     build and run every check in tests/
#   make clean

CC      ?= cc
CFLAGS  ?= -std=c99 -O2 -Wall -Wextra -pedantic -ffp-contract=off

SOURCES := $(wildcard src/score/*.c src/compose/*.c src/synth/*.c src/output/*.c)
HEADERS := $(wildcard src/*/*.h)

chimes: src/main.c $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -Isrc -o $@ src/main.c $(SOURCES) -lm

test: chimes
	sh tests/run.sh

clean:
	rm -rf chimes tests/bin tests/tmp

.PHONY: test clean
