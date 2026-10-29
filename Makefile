CC = gcc
CFLAGS = -std=c99 -g -Wall -Wextra -pedantic
LDLIBS = -lm

SRCS = main.c modules.c tokenize.c parse.c type.c codegen.c x64.c buffer.c program.c builtin.c pe.c elf.c unicode.c types.c integer_bases.c wide_runtime.c grapheme_runtime.c
OBJS = $(SRCS:.c=.o)
DEPS = $(OBJS:.o=.d)

all: qyc

qyc: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)

clean:
	rm -f qyc qyc.exe $(OBJS) $(DEPS)

.PHONY: all clean
