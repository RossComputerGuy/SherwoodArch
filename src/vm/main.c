#include <savm/vm.h>
#include <sys/types.h>
#include <argp.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

const char* argp_program_version = "savm 1.0";
const char* argp_program_bug_address = "<spaceboyross@yandex.com>";

static char doc[] = "SAVM -- The official virtual machine for the Sherwood Architecture";
static char args_doc[] = "[options]";

static struct argp_option options[] = {
	{"clockspeed",'c',"NUMBER",0,"Sets the CPU's clockspeed"},
	{"firmware",'f',"FILE",0,"Loads firmware from a file"},
	{"ram",'r',"FILE",0,"Load and save RAM to/from file (Start vector is overrided by firmware)"},
	{0}
};

struct arguments {
	int clockspeed;
	char* firmware;
	char* ram;
};

static error_t parse_opt(int key,char* arg,struct argp_state* state) {
	struct arguments* arguments = state->input;
	switch(key) {
		case 'c':
			arguments->clockspeed = atoi(arg);
			break;
		case 'f':
			arguments->firmware = arg;
			break;
		case 'r':
			arguments->ram = arg;
			break;
		default: return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

int main(int argc,char** argv) {
	struct arguments arguments;
	arguments.clockspeed = 1;
	
	argp_parse(&argp,argc,argv,0,0,&arguments);
	
	savm_t vm;
	savm_error_e err = savm_create(&vm);
	if(err != SAVM_ERROR_NONE) {
		printf("savm_create(): %d\n",err);
		return EXIT_FAILURE;
	}
	
	if(arguments.ram != NULL) {
		if(access(arguments.ram,F_OK) != -1) {
			err = savm_ioctl_loadfile(&vm,SAVM_IO_RAM_BASE,arguments.ram);
			if(err != SAVM_ERROR_NONE) {
				printf("savm_ioctl_loadfile(vm,0x%x,\"%s\"): %d\n",SAVM_IO_RAM_BASE,arguments.ram,err);
				return EXIT_FAILURE;
			}
		} else {
			fprintf(stderr,"Failed to open \"%s\" and load it into memory\n",arguments.ram);
			return EXIT_FAILURE;
		}
	}
	
	if(arguments.firmware != NULL) {
		if(access(arguments.firmware,F_OK) != -1) {
			err = savm_ioctl_loadfile(&vm,SAVM_IO_RAM_BASE,arguments.firmware);
			if(err != SAVM_ERROR_NONE) {
				printf("savm_ioctl_loadfile(vm,0x%x,\"%s\"): %d\n",SAVM_IO_RAM_BASE,arguments.firmware,err);
				return EXIT_FAILURE;
			}
		} else {
			fprintf(stderr,"Failed to open \"%s\" and load it as firmware\n",arguments.firmware);
			return EXIT_FAILURE;
		}
	}
	
	vm.cpu.cores[0].running = 1;
	
	while(vm.cpu.cores[0].running) {
		int msec = 0;
		clock_t before = clock();
		do {
			err = savm_cpu_cycle(&vm);
			if(err != SAVM_ERROR_NONE) {
				printf("savm_cpu_cycle(): %d\n",err);
				vm.cpu.cores[0].running = 0;
				break;
			}
			clock_t difference = clock()-before;
			msec = difference*1000/CLOCKS_PER_SEC;
		} while(msec < arguments.clockspeed);
	}
	
	if(arguments.ram != NULL) {
		err = savm_ioctl_dumpfile(&vm,SAVM_IO_RAM_BASE,SAVM_IO_RAM_SIZE,arguments.ram);
		if(err != SAVM_ERROR_NONE) {
			printf("savm_ioctl_dumpfile(vm,0x%x,0x%x,\"%s\"): %d\n",SAVM_IO_RAM_BASE,SAVM_IO_RAM_SIZE,arguments.ram,err);
			return EXIT_FAILURE;
		}
	}
	
	err = savm_destroy(&vm);
	if(err != SAVM_ERROR_NONE) {
		printf("savm_destroy(): %d\n",err);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
