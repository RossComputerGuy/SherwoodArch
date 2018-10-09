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
	
	if(argc == 1) {
		vm.io.ram[0] = 44;
		vm.io.ram[1] = SAVM_IO_UART_BASE+1;
		vm.io.ram[2] = 'H';
		
		vm.io.ram[3] = 44;
		vm.io.ram[4] = SAVM_IO_UART_BASE+1;
		vm.io.ram[5] = 'e';
		
		vm.io.ram[6] = 44;
		vm.io.ram[7] = SAVM_IO_UART_BASE+1;
		vm.io.ram[8] = 'l';
		
		vm.io.ram[9] = 44;
		vm.io.ram[10] = SAVM_IO_UART_BASE+1;
		vm.io.ram[11] = 'l';
		
		vm.io.ram[12] = 44;
		vm.io.ram[13] = SAVM_IO_UART_BASE+1;
		vm.io.ram[14] = 'o';
		
		vm.io.ram[15] = 44;
		vm.io.ram[16] = SAVM_IO_UART_BASE+1;
		vm.io.ram[17] = ' ';
		
		vm.io.ram[18] = 44;
		vm.io.ram[19] = SAVM_IO_UART_BASE+1;
		vm.io.ram[20] = 'W';
		
		vm.io.ram[21] = 44;
		vm.io.ram[22] = SAVM_IO_UART_BASE+1;
		vm.io.ram[23] = 'o';
		
		vm.io.ram[24] = 44;
		vm.io.ram[25] = SAVM_IO_UART_BASE+1;
		vm.io.ram[26] = 'r';
		
		vm.io.ram[27] = 44;
		vm.io.ram[28] = SAVM_IO_UART_BASE+1;
		vm.io.ram[29] = 'l';
		
		vm.io.ram[30] = 44;
		vm.io.ram[31] = SAVM_IO_UART_BASE+1;
		vm.io.ram[32] = 'd';
		
		vm.io.ram[33] = 44;
		vm.io.ram[34] = SAVM_IO_UART_BASE+1;
		vm.io.ram[35] = '\n';
	} else {
		err = savm_ioctl_loadfile(&vm,SAVM_IO_RAM_BASE,argv[1]);
		if(err != SAVM_ERROR_NONE) {
			printf("savm_ioctl_loadfile(vm,0x%x,\"%s\"): %d\n",SAVM_IO_RAM_BASE,argv[1],err);
			return EXIT_FAILURE;
		}
	}
	
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
