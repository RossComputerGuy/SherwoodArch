BIN=./bin
SRC=./src

ASSEMBLER_BIN=$(BIN)/assembler
VM_BIN=$(BIN)/vm

ASSEMBLER_SRC=$(SRC)/assembler
BUILD_SRC=$(SRC)/build
INCLUDE_SRC=$(SRC)/include
VM_SRC=$(SRC)/vm

all: vm assembler

clean:
	@rm -rf $(BIN)

include $(BUILD_SRC)/Makefile
