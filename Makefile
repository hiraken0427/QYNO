CC = gcc
CFLAGS = -std=c99 -O2 -g -Wall -Wextra -pedantic
LDLIBS = -lm

SRCS = main.c diagnostics.c messages.c modules.c tokenize.c parse.c type.c optimizer.c opt_effects.c opt_inline.c opt_memory.c opt_range.c opt_transfer.c machine_ir.c machine_alloc.c codegen.c x64.c buffer.c program.c builtin.c pe.c elf.c unicode.c types.c integer_bases.c wide_runtime.c grapheme_runtime.c
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

# Regenerate the freestanding x64 task engine. Normal builds use async_blob.h.
async-blob:
	@set -eu; asynctmp=$$(mktemp -d); trap 'rm -rf "$$asynctmp"' EXIT; \
	$(CC) -std=c99 -O2 -fPIC -fvisibility=hidden -ffreestanding -fno-builtin -fno-stack-protector -fno-asynchronous-unwind-tables -fno-jump-tables -mno-red-zone -Wall -Wextra -c async_runtime.c -o "$$asynctmp/runtime.o"; \
	printf '%s\n' 'ENTRY(qyno_async)' 'SECTIONS { . = 0; .text : { *(.text .text.*) *(.rodata .rodata.*) } /DISCARD/ : { *(.eh_frame*) *(.comment) *(.note*) } }' > "$$asynctmp/runtime.ld"; \
	ld -T "$$asynctmp/runtime.ld" --gc-sections -o "$$asynctmp/runtime.elf" "$$asynctmp/runtime.o"; \
	test -z "$$(nm -u "$$asynctmp/runtime.elf")"; \
	objcopy -O binary --only-section=.text "$$asynctmp/runtime.elf" "$$asynctmp/runtime.bin"; \
	python3 -c 'import pathlib,subprocess,sys; p=pathlib.Path(sys.argv[1]); sections=subprocess.check_output(["size",str(p/"runtime.elf")],text=True).splitlines()[-1].split(); assert sections[1:3]==["0","0"]; symbols=subprocess.check_output(["nm",str(p/"runtime.elf")],text=True).splitlines(); entry=next(int(s.split()[0],16) for s in symbols if s.endswith(" T qyno_async")); data=(p/"runtime.bin").read_bytes(); lines=["/* Generated from async_runtime.c with GCC -O2. */",f"#define ASYNC_ENTRY_OFFSET {entry}","static const unsigned char async_blob[] = {"]; lines += ["    "+",".join(f"0x{b:02x}" for b in data[i:i+16])+"," for i in range(0,len(data),16)]; pathlib.Path("async_blob.h").write_text("\n".join(lines+["};",""]))' "$$asynctmp"

.PHONY: async-blob
