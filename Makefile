BIN=./bin
SRC=./src

VM_BIN=$(BIN)/vm

BUILD_SRC=$(SRC)/build
INCLUDE_SRC=$(SRC)/include
VM_SRC=$(SRC)/vm

all: vm

clean:
	@rm -rf $(BIN)

include $(BUILD_SRC)/Makefile
