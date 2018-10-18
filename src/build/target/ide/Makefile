$(IDE_BIN): $(IDE_SRC)
	@cp $(BIN)/sa.js $(IDE_SRC)/src/libs/sa.js
	@cd $(IDE_SRC) && npm install --save && npm run make
	@rm -rf $(IDE_BIN)
	@mv $(IDE_SRC)/out $(IDE_BIN)
	@cp $(IDE_BIN)/make/* $(BIN)/

ide: $(IDE_BIN)
