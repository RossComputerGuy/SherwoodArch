#ifndef __SAVM_VM_H_
#define __SAVM_VM_H_ 1

#include <savm/error.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef SAVM_CPU_REG_FLAG_INTR
#define SAVM_CPU_REG_FLAG_INTR (1 << 0)
#endif

#ifndef SAVM_CPU_STACK_SIZE
#define SAVM_CPU_STACK_SIZE 20
#endif

#ifndef SAVM_IO_RAM_BASE
#define SAVM_IO_RAM_BASE 0xA0000000
#endif

#ifndef SAVM_IO_RAM_END
#define SAVM_IO_RAM_END 0xFFFFFFFF
#endif

struct savm;

typedef uint64_t (*savm_ioctl_read_p)(struct savm* vm,uint64_t i);
typedef void (*savm_ioctl_write_p)(struct savm* vm,uint64_t i,uint64_t data);

typedef struct {
	uint64_t addr;
	uint64_t end;
	
	savm_ioctl_read_p read;
	savm_ioctl_write_p write;
} savm_io_mmap_entry_t;

typedef struct savm {
	struct {
		struct {
			uint64_t sp; /* Stack Pointer */
			uint64_t flags; /* Flags */
			uint64_t ip; /* Instruction Pointer */
			uint64_t pc; /* Program Counter */
		} regs;
		
		uint64_t stack[SAVM_CPU_STACK_SIZE];
		int running;
	} cpu;
	struct {
		uint64_t* ram;
		
		size_t mmapSize;
		savm_io_mmap_entry_t* mmap;
	} io;
} savm_t;

savm_error_e savm_create(savm_t* vm);
savm_error_e savm_destroy(savm_t* vm);
savm_error_e savm_reset(savm_t* vm);

savm_error_e savm_cpu_regread(savm_t* vm,uint64_t i,uint64_t* val);
savm_error_e savm_cpu_regwrite(savm_t* vm,uint64_t i,uint64_t val);
savm_error_e savm_cpu_cycle(savm_t* vm);

savm_error_e savm_ioctl_mmap(savm_t* vm,uint64_t addr,uint64_t end,savm_ioctl_read_p read,savm_ioctl_write_p write);
savm_error_e savm_ioctl_read(savm_t* vm,uint64_t addr,uint64_t* data);
savm_error_e savm_ioctl_write(savm_t* vm,uint64_t addr,uint64_t data);

#endif
