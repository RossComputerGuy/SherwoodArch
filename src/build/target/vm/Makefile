VM_OBJECTS=$(VM_BIN)/main.o $(VM_BIN)/vm.o

$(VM_BIN)/%.o: $(VM_SRC)/%.c
	@echo "CC $<"
	@mkdir -p $(@D)
	@$(CC) $(VM_CFLAGS) -o $@ $<

$(BIN)/vm.bin: $(VM_OBJECTS)
	@echo "LD $@"
	@$(LD) $(VM_LDFLAGS) -o $@ $(VM_OBJECTS)

vm: $(BIN)/vm.bin
