BIN=./bin
SRC=./src

IDE_BIN=$(BIN)/ide

BUILD_SRC=$(SRC)/build
IDE_SRC=$(SRC)/ide
INCLUDE_SRC=$(SRC)/include
SAJS_SRC=$(SRC)/package

all: sajs ide

clean:
	@rm -rf $(BIN) $(IDE_SRC)/out $(SAJS_SRC)/dist $(IDE_SRC)/node_modules $(SAJS_SRC)/node_modules $(IDE_SRC)/package-lock.json $(SAJS_SRC)/package-lock.json

include $(BUILD_SRC)/Makefile
