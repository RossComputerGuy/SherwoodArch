CC=gcc
LD=gcc

CFLAGS=-I $(INCLUDE_SRC)

ASSEMBLER_CFLAGS=-c $(CFLAGS) -g
VM_CFLAGS=-c $(CFLAGS)

include $(BUILD_SRC)/target/assembler/Makefile
include $(BUILD_SRC)/target/vm/Makefile
