ASSEMBLER_OBJECTS=$(ASSEMBLER_BIN)/main.o $(ASSEMBLER_BIN)/opcode.o

$(ASSEMBLER_BIN)/%.o: $(ASSEMBLER_SRC)/%.c
	@echo "CC $<"
	@mkdir -p $(@D)
	@$(CC) $(ASSEMBLER_CFLAGS) -o $@ $<

$(BIN)/sasmc.bin: $(ASSEMBLER_OBJECTS)
	@echo "LD $@"
	@$(LD) $(ASSEMBLER_LDFLAGS) -o $@ $(ASSEMBLER_OBJECTS)

assembler: $(BIN)/sasmc.bin
