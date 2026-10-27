CC = gcc
CFLAGS = -std=c99 -g -Wall -Wextra -pedantic
PYTHON ?= python3
SRCS = main.c modules.c tokenize.c parse.c type.c codegen.c x64.c buffer.c program.c builtin.c pe.c elf.c unicode.c types.c integer_bases.c
OBJS = $(SRCS:.c=.o)

qyc: $(OBJS)
	$(CC) -o $@ $(OBJS) -lm

$(OBJS): qyc.h
codegen.o: collections_codegen.inc memory_codegen.inc places_codegen.inc models_codegen.inc file_runtime.inc integer_bases_codegen.inc
type.o: arrays_type.inc models_type.inc readonly_type.inc const_models.inc initialization_type.inc initialization_flow.inc interfaces_type.inc interfaces_specialize.inc integer_bases_type.inc file_close_analysis.inc inference_warnings.inc return_inference.inc

test: qyc
	PYTHON="$(PYTHON)" bash ./test.sh
	$(PYTHON) tests/rounding_test.py ./qyc
	$(PYTHON) tests/semantics_test.py ./qyc
	$(PYTHON) tests/text_test.py ./qyc
	$(PYTHON) tests/arrays_test.py ./qyc
	$(PYTHON) tests/modules_test.py ./qyc
	$(PYTHON) tests/io_test.py ./qyc
	$(PYTHON) tests/for_test.py ./qyc
	$(PYTHON) tests/input_refs_test.py ./qyc
	$(PYTHON) tests/file_test.py ./qyc
	$(PYTHON) tests/model_test.py ./qyc
	$(PYTHON) tests/visibility_test.py ./qyc
	$(PYTHON) tests/readonly_test.py ./qyc
	$(PYTHON) tests/initialization_test.py ./qyc
	$(PYTHON) tests/interfaces_test.py ./qyc
	$(PYTHON) tests/bitwise_test.py ./qyc
	$(PYTHON) tests/integer_bases_test.py ./qyc
	$(PYTHON) tests/auto_test.py ./qyc
	$(PYTHON) tests/implicit_declarations_test.py ./qyc
	$(PYTHON) tests/global_const_inference_test.py ./qyc
	$(PYTHON) tests/return_inference_test.py ./qyc

test-return-inference: qyc
	$(PYTHON) tests/return_inference_test.py ./qyc

test-global-const-inference: qyc
	$(PYTHON) tests/global_const_inference_test.py ./qyc

test-implicit-declarations: qyc
	$(PYTHON) tests/implicit_declarations_test.py ./qyc

test-auto: qyc
	$(PYTHON) tests/auto_test.py ./qyc

test-bitwise: qyc
	$(PYTHON) tests/bitwise_test.py ./qyc

test-integer-bases: qyc
	$(PYTHON) tests/integer_bases_test.py ./qyc

test-interfaces: qyc
	$(PYTHON) tests/interfaces_test.py ./qyc

test-initialization: qyc
	$(PYTHON) tests/initialization_test.py ./qyc

test-readonly: qyc
	$(PYTHON) tests/readonly_test.py ./qyc

test-visibility: qyc
	$(PYTHON) tests/visibility_test.py ./qyc

test-model: qyc
	$(PYTHON) tests/model_test.py ./qyc

test-file: qyc
	$(PYTHON) tests/file_test.py ./qyc

test-input-refs: qyc
	$(PYTHON) tests/input_refs_test.py ./qyc

test-for: qyc
	$(PYTHON) tests/for_test.py ./qyc

test-io: qyc
	$(PYTHON) tests/io_test.py ./qyc

test-modules: qyc
	$(PYTHON) tests/modules_test.py ./qyc

test-lineedit: qyc
	$(PYTHON) tests/lineedit_test.py ./qyc

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

.PHONY: test test-auto test-implicit-declarations test-global-const-inference test-return-inference test-bitwise test-integer-bases test-interfaces test-initialization test-readonly test-visibility test-model test-file test-input-refs test-for test-io test-modules test-lineedit test-rounding test-semantics test-text test-arrays clean
