 $(BIN)/sajs.tar.gz: $(SAJS_SRC)
	@mkdir -p $(BIN)
	@cd $(SAJS_SRC) && npm run build && npm install --save
	@cp $(SAJS_SRC)/dist/* $(BIN)/
	@cd $(SAJS_SRC) && rm -rf node_modules package-lock.json
	@cd $(SAJS_SRC) && tar -zcf sajs.tar.gz *
	@mv $(SAJS_SRC)/sajs.tar.gz $(BIN)/sajs.tar.gz

sajs: $(BIN)/sajs.tar.gz
