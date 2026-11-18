CC = gcc
CFLAGS = -std=c99 -O2 -g -Wall -Wextra -pedantic
LDLIBS = -lm
.DEFAULT_GOAL := all

SRCS = \
	src/core/main.c \
	src/i18n/diagnostics.c \
	src/i18n/messages.c \
	src/modules/modules.c \
	src/syntax/tokenize.c \
	src/syntax/parse.c \
	src/analysis/type.c \
	src/optimizer/optimizer.c \
	src/optimizer/opt_effects.c \
	src/optimizer/opt_inline.c \
	src/optimizer/opt_memory.c \
	src/optimizer/opt_range.c \
	src/optimizer/opt_transfer.c \
	src/backend/machine-ir/machine_ir.c \
	src/backend/register-allocation/machine_alloc.c \
	src/backend/codegen.c \
	src/backend/x64/x64.c \
	src/utils/buffer.c \
	src/backend/program.c \
	src/modules/builtin.c \
	src/backend/executable/pe.c \
	src/backend/executable/elf.c \
	src/utils/unicode.c \
	src/analysis/types.c \
	src/features/numeric/integer_bases.c \
	src/features/numeric/wide_runtime.c \
	src/features/text/grapheme_runtime.c
EXEEXT ?= $(if $(filter Windows_NT,$(OS)),.exe,)
TARGET = bin/qyc$(EXEEXT)
OBJS = $(patsubst %.c,build/%.o,$(SRCS))
DEPS = $(OBJS:.o=.d)

all: $(TARGET)

qyc: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -Iinclude $(CFLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)

clean:
	rm -rf build bin

.PHONY: all qyc clean

# Regenerate the freestanding x64 task engine. Normal builds use async_blob.h.
async-blob:
	@set -eu; asynctmp=$$(mktemp -d); trap 'rm -rf "$$asynctmp"' EXIT; \
	$(CC) -std=c99 -O2 -fPIC -fvisibility=hidden -ffreestanding -fno-builtin -fno-stack-protector -fno-asynchronous-unwind-tables -fno-jump-tables -mno-red-zone -Wall -Wextra -c src/features/tasks/async_runtime.c -o "$$asynctmp/runtime.o"; \
	printf '%s\n' 'ENTRY(qyno_async)' 'SECTIONS { . = 0; .text : { *(.text .text.*) *(.rodata .rodata.*) } /DISCARD/ : { *(.eh_frame*) *(.comment) *(.note*) } }' > "$$asynctmp/runtime.ld"; \
	$(CC) -std=c99 -O2 -fPIC -fvisibility=hidden -ffreestanding -fno-builtin -fno-stack-protector -fno-asynchronous-unwind-tables -fno-jump-tables -mno-red-zone -maccumulate-outgoing-args -Wall -Wextra -c src/features/file-io/file_async_runtime.c -o "$$asynctmp/file.o"; \
	ld -T "$$asynctmp/runtime.ld" --gc-sections -o "$$asynctmp/runtime.elf" "$$asynctmp/runtime.o" "$$asynctmp/file.o"; \
	test -z "$$(nm -u "$$asynctmp/runtime.elf")"; \
	objcopy -O binary --only-section=.text "$$asynctmp/runtime.elf" "$$asynctmp/runtime.bin"; \
	python3 -c 'import pathlib,subprocess,sys; p=pathlib.Path(sys.argv[1]); sections=subprocess.check_output(["size",str(p/"runtime.elf")],text=True).splitlines()[-1].split(); assert sections[1:3]==["0","0"]; symbols=subprocess.check_output(["nm",str(p/"runtime.elf")],text=True).splitlines(); entry=next(int(s.split()[0],16) for s in symbols if s.endswith(" T qyno_async")); data=(p/"runtime.bin").read_bytes(); lines=["/* Generated from task and file worker runtimes with GCC -O2. */",f"#define ASYNC_ENTRY_OFFSET {entry}","static const unsigned char async_blob[] = {"]; lines += ["    "+",".join(f"0x{b:02x}" for b in data[i:i+16])+"," for i in range(0,len(data),16)]; pathlib.Path("generated/async_blob.h").write_text("\n".join(lines+["};",""]))' "$$asynctmp"

.PHONY: async-blob

# File workers share the task-engine image and its private ABI.
async-file-blob: async-blob

.PHONY: async-file-blob
