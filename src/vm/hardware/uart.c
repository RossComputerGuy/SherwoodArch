#include <savm/hardware/uart.h>
#include <savm/vm.h>

uint64_t savm_uart_read(savm_t* vm,uint64_t i);
void savm_uart_write(savm_t* vm,uint64_t i,uint64_t data);

savm_error_e savm_uart_create(savm_uart_t* uart) {
	memset(uart,0,sizeof(savm_uart_t));
	
	uart->out = stdout;
	uart->in = stdin;
	return SAVM_ERROR_NONE;
}

savm_error_e savm_uart_destroy(savm_uart_t* uart) {
	if(uart->out != stdout) fclose(uart->out);
	if(uart->in != stdin) fclose(uart->in);
	return SAVM_ERROR_NONE;
}

savm_error_e savm_uart_reset(savm_uart_t* uart,savm_t* vm) {
	uart->input = 0;
	return savm_ioctl_mmap(vm,SAVM_IO_UART_BASE,SAVM_IO_UART_END,savm_uart_read,savm_uart_write);
}

savm_error_e savm_uart_cycle(savm_uart_t* uart,savm_t* vm) {
	//uart->input = getc(uart->in);
	return SAVM_ERROR_NONE;
}

uint64_t savm_uart_read(savm_t* vm,uint64_t i) {
	switch(i) {
		case 0: return vm->uart.input != 0;
		case 1: return vm->uart.input;
		case 2: return 1; // Is out ready
	}
	return 0;
}

void savm_uart_write(savm_t* vm,uint64_t i,uint64_t data) {
	switch(i) {
		case 1:
			putc((char)data,vm->uart.out);
			break;
	}
}
