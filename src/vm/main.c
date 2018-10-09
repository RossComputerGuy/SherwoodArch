#include <savm/vm.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc,char** argv) {
	savm_t vm;
	savm_error_e err = savm_create(&vm);
	if(err != SAVM_ERROR_NONE) {
		printf("savm_create(): %d\n",err);
		return EXIT_FAILURE;
	}
	
	vm.io.ram[0] = 44;
	vm.io.ram[1] = SAVM_IO_UART_BASE+1;
	vm.io.ram[2] = 'H';
	
	vm.io.ram[3] = 44;
	vm.io.ram[4] = SAVM_IO_UART_BASE+1;
	vm.io.ram[5] = 'e';
	
	vm.cpu.running = 1;
	
	// TODO: load firmware
	while(vm.cpu.running) {
		err = savm_cpu_cycle(&vm);
		if(err != SAVM_ERROR_NONE) {
			printf("savm_cpu_cycle(): %d\n",err);
			vm.cpu.running = 0;
			break;
		}
	}
	err = savm_destroy(&vm);
	if(err != SAVM_ERROR_NONE) {
		printf("savm_destroy(): %d\n",err);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
