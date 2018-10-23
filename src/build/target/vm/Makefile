VM_OBJECTS += $(VM_BIN)/hardware/mailbox.o $(VM_BIN)/hardware/rtc.o $(VM_BIN)/hardware/uart.o
VM_OBJECTS += $(VM_BIN)/crc32.o $(VM_BIN)/main.o $(VM_BIN)/utils.o $(VM_BIN)/vm.o

$(VM_BIN)/%.o: $(VM_SRC)/%.c
	@echo "CC $<"
	@mkdir -p $(@D)
	@$(CC) $(VM_CFLAGS) -o $@ $<

$(BIN)/vm.bin: $(VM_OBJECTS)
	@echo "LD $@"
	@$(LD) $(VM_LDFLAGS) -o $@ $(VM_OBJECTS)

vm: $(BIN)/vm.bin
