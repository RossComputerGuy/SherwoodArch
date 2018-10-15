ide:
	@cd $(IDE_SRC) && npm install --save-dev && npm run pack && npm run dist
	@rm -rf $(IDE_BIN)
	@mv $(IDE_SRC)/dist/* $(IDE_BIN)
