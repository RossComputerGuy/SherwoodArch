BIN=./bin
SRC=./src

ASSEMBLER_BIN=$(BIN)/assembler
VM_BIN=$(BIN)/vm

ASSEMBLER_SRC=$(SRC)/assembler
BUILD_SRC=$(SRC)/build
INCLUDE_SRC=$(SRC)/include
SAJS_SRC=$(SRC)/package
VM_SRC=$(SRC)/vm

all: vm assembler sajs

clean:
	@rm -rf $(BIN) $(SAJS_SRC)/dist $(SAJS_SRC)/node_modules $(SAJS_SRC)/package-lock.json

include $(BUILD_SRC)/Makefile
