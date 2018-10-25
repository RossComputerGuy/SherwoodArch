$(IDE_BIN): $(IDE_SRC)
	@mkdir -p $(BIN)
	@cd $(IDE_SRC) && npm install --save
	@cp -r $(SAJS_SRC) $(IDE_SRC)/node_modules/sajs && cd $(IDE_SRC)/node_modules/sajs && npm install --save
	@cd $(IDE_SRC) && npm run make
	@rm -rf $(IDE_BIN)
	@mv $(IDE_SRC)/out $(IDE_BIN)
	@cp $(IDE_BIN)/make/* $(BIN)/

ide: $(IDE_BIN)
