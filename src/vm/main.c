#include <savm/vm.h>
#include <stdlib.h>
#include <stdio.h>

int main(int argc,char** argv) {
	savm_t vm;
	savm_error_e err = savm_create(&vm);
	if(err != SAVM_ERROR_NONE) {
		printf("Error: %d\n",err);
		return EXIT_FAILURE;
	}
	// TODO: load firmware
	// TODO: start CPU
	while(vm.cpu.running) {
		err = savm_cpu_cycle(&vm);
		if(err != SAVM_ERROR_NONE) {
			printf("Error: %d\n",err);
			vm.cpu.running = 0;
			break;
		}
	}
	err = savm_destroy(&vm);
	if(err != SAVM_ERROR_NONE) {
		printf("Error: %d\n",err);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
