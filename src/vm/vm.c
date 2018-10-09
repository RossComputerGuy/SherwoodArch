#include <savm/error.h>
#include <savm/vm.h>
#include <string.h>

uint64_t savm_ioctl_ram_read(savm_t* vm,uint64_t i);
void savm_ioctl_ram_write(savm_t* vm,uint64_t i,uint64_t data);

savm_error_e savm_create(savm_t* vm) {
	memset(vm,0,sizeof(savm_t));
	return savm_reset(vm);
}

savm_error_e savm_destroy(savm_t* vm) {
	if(vm->io.ram != NULL) free(vm->io.ram);
	if(vm->io.mmap != NULL) free(vm->io.mmap);
    return SAVM_ERROR_NONE;
}

savm_error_e savm_reset(savm_t* vm) {
	/* (Re)initialize the RAM */
	if(vm->io.ram != NULL) free(vm->io.ram);
	vm->io.ram = malloc(sizeof(uint64_t)*(SAVM_IO_RAM_END-SAVM_IO_RAM_BASE));
	if(vm->io.ram == NULL) return SAVM_ERROR_MEM;
	
	/* (Re)initialize the memory map in the IO Controller */
	if(vm->io.mmap != NULL) free(vm->io.mmap);
	savm_error_e err = savm_ioctl_mmap(vm,SAVM_IO_RAM_BASE,SAVM_IO_RAM_END,savm_ioctl_ram_read,savm_ioctl_ram_write);
	if(err != SAVM_ERROR_NONE) return err;
	
	/* Reset the registers */
	vm->cpu.regs.pc = SAVM_IO_RAM_BASE;
	
	/* Reset the CPU */
	memset(vm->cpu.stack,0,sizeof(uint64_t)*SAVM_CPU_STACK_SIZE);
	vm->cpu.running = 0;
	return SAVM_ERROR_NONE;
}

/* CPU */
savm_error_e savm_cpu_regread(savm_t* vm,uint64_t i,uint64_t* val) {
	switch(i) {
		case 0: /* flags*/
			*val = vm->cpu.regs.flags;
			break;
		default: return SAVM_ERROR_INVAL_ADDR;
	}
	return SAVM_ERROR_NONE;
}

savm_error_e savm_cpu_regwrite(savm_t* vm,uint64_t i,uint64_t val) {
	switch(i) {
		case 0: /* flags*/
			vm->cpu.regs.flags = val;
			break;
		default: return SAVM_ERROR_INVAL_ADDR;
	}
	return SAVM_ERROR_NONE;
}

savm_error_e savm_cpu_cycle(savm_t* vm) {
	/* Fetches the instruction from memory */
	savm_error_e err = savm_ioctl_read(vm,vm->cpu.regs.pc,&vm->cpu.regs.ip);
	if(err != SAVM_ERROR_NONE) return err;
	
	/* Decodes the instruction */
	uint64_t instr = vm->cpu.regs.ip;
    uint64_t addr;
    uint64_t val;
    
	err = savm_ioctl_read(vm,vm->cpu.regs.pc+1,&addr);
	if(err != SAVM_ERROR_NONE) return err;
    
    err = savm_ioctl_read(vm,vm->cpu.regs.pc+2,&val);
    if(err != SAVM_ERROR_NONE) return err;
	
	/* Execute the instruction */
	switch(instr) {
		case 0: /* NOP */ break;
		case 1: /* ADDR */ break;
		case 2: /* ADDM */ break;
		case 3: /* SUBR */ break;
		case 4: /* SUBM */ break;
		case 5: /* MULR */ break;
		case 6: /* MULM */ break;
		case 7: /* DIVR */ break;
		case 8: /* DIVM */ break;
		case 9: /* ANDR */ break;
		case 10: /* ANDM */ break;
		case 11: /* ORR */ break;
		case 12: /* ORM */ break;
		case 13: /* XORR */ break;
		case 14: /* XORM */ break;
		case 15: /* NORR */ break;
		case 16: /* NORM */ break;
		case 17: /* CMPR */ break;
		case 18: /* CMPM */ break;
		case 19: /* JITR */ break;
		case 20: /* JITM */ break;
		case 21: /* JMPR */ break;
		case 22: /* JMPM */ break;
		case 23: /* CALLR */ break;
		case 24: /* CALLM */ break;
		case 25: /* RET */
			if(vm->cpu.regs.sp < SAVM_CPU_STACK_SIZE) vm->cpu.regs.pc = vm->cpu.stack[vm->cpu.regs.sp--]-1;
			// TODO: exception
			break;
		case 26: /* PUSHR */
			if(vm->cpu.regs.sp < SAVM_CPU_STACK_SIZE) {
				err = savm_cpu_regread(vm,addr,&vm->cpu.stack[vm->cpu.regs.sp++]);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					// TODO: exception
				}
			}
			break;
		case 27: /* PUSHM */
			if(vm->cpu.regs.sp < SAVM_CPU_STACK_SIZE) {
				err = savm_ioctl_read(vm,addr,&vm->cpu.stack[vm->cpu.regs.sp++]);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					// TODO: exception
				}
			}
			break;
		case 28: /* POPR */ break;
		case 29: /* POPM */ break;
		case 30: /* MOVRR */ break;
		case 31: /* MOVRM */ break;
		case 32: /* MOVMR */ break;
		case 33: /* MOVMM */ break;
		case 34: /* STOR */ break;
		case 35: /* STOM */ break;
		case 36: /* INTR */
			if(vm->cpu.regs.flags & SAVM_CPU_REG_FLAG_INTR) {} // TODO: exception
			vm->cpu.regs.flags |= SAVM_CPU_REG_FLAG_INTR;
			break;
		case 37: /* INTM */
			if(vm->cpu.regs.flags & SAVM_CPU_REG_FLAG_INTR) {} // TODO: exception
			vm->cpu.regs.flags |= SAVM_CPU_REG_FLAG_INTR;
			break;
		case 38: /* LDITBLR */ break;
		case 39: /* LDITBLM */ break;
		case 40: /* HLT */
			vm->cpu.running = 0;
			break;
		default:
			/* TODO: invalid instruction */
			break;
	}
	
	vm->cpu.regs.pc += 3;
	return SAVM_ERROR_NONE;
}

/* IO Controller */
savm_error_e savm_ioctl_mmap(savm_t* vm,uint64_t addr,uint64_t end,savm_ioctl_read_p read,savm_ioctl_write_p write) {
	uint64_t tmp;
	if(savm_ioctl_read(vm,addr,&tmp) != SAVM_ERROR_NONE) return SAVM_ERROR_MAPPED;
	size_t i = -1;
	if(vm->io.mmapSize < 1 || vm->io.mmap == NULL) {
		vm->io.mmapSize = 1;
		vm->io.mmap = malloc(sizeof(savm_io_mmap_entry_t));
		i = 0;
	}
	if(i == -1) {
		i = vm->io.mmapSize++;
		vm->io.mmap = realloc(vm->io.mmap,sizeof(savm_io_mmap_entry_t)*vm->io.mmapSize);
	}
	if(vm->io.mmap == NULL) return SAVM_ERROR_MEM;
	vm->io.mmap[i].addr = addr;
	vm->io.mmap[i].end = end;
	vm->io.mmap[i].read = read;
	vm->io.mmap[i].write = write;
	return SAVM_ERROR_NONE;
}

savm_error_e savm_ioctl_read(savm_t* vm,uint64_t addr,uint64_t* data) {
	if(vm->io.mmap == NULL) return SAVM_ERROR_NOTMAPPED;
	for(size_t i = 0;i < vm->io.mmapSize;i++) {
		if(vm->io.mmap[i].addr >= addr && vm->io.mmap[i].end > addr) {
			*data = vm->io.mmap[i].read(vm,vm->io.mmap[i].addr-addr);
			return SAVM_ERROR_NONE;
		}
	}
	return SAVM_ERROR_NOTMAPPED;
}

savm_error_e savm_ioctl_write(savm_t* vm,uint64_t addr,uint64_t data) {
	if(vm->io.mmap == NULL) return SAVM_ERROR_NOTMAPPED;
	for(size_t i = 0;i < vm->io.mmapSize;i++) {
		if(vm->io.mmap[i].addr >= addr && vm->io.mmap[i].end > addr) {
			vm->io.mmap[i].write(vm,vm->io.mmap[i].addr-addr,data);
			return SAVM_ERROR_NONE;
		}
	}
	return SAVM_ERROR_NOTMAPPED;
}

uint64_t savm_ioctl_ram_read(savm_t* vm,uint64_t i) {
	return vm->io.ram[i];
}

void savm_ioctl_ram_write(savm_t* vm,uint64_t i,uint64_t data) {
	vm->io.ram[i] = data;
}
