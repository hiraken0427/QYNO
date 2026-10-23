CC = gcc
CFLAGS = -std=c99 -g -Wall -Wextra -pedantic
PYTHON ?= python3
SRCS = main.c tokenize.c parse.c type.c codegen.c x64.c buffer.c program.c builtin.c pe.c elf.c unicode.c types.c
OBJS = $(SRCS:.c=.o)

qyc: $(OBJS)
	$(CC) -o $@ $(OBJS) -lm

$(OBJS): qyc.h
codegen.o: collections_codegen.inc memory_codegen.inc places_codegen.inc
type.o: arrays_type.inc

test: qyc
	./test.sh

test-rounding: qyc
	$(PYTHON) tests/rounding_test.py ./qyc

test-semantics: qyc
	$(PYTHON) tests/semantics_test.py ./qyc

test-text: qyc
	$(PYTHON) tests/text_test.py ./qyc

test-arrays: qyc
	$(PYTHON) tests/arrays_test.py ./qyc

clean:
	rm -f qyc qyc.exe *.o tmp tmp.exe tmp.qy tmp.s

.PHONY: test test-rounding test-semantics test-text test-arrays clean
