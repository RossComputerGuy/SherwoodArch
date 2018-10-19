#include <savm/error.h>
#include <savm/vm.h>
#include <sys/stat.h>
#include <string.h>

uint64_t savm_ioctl_ioctl_read(savm_t* vm,uint64_t i);
void savm_ioctl_ioctl_write(savm_t* vm,uint64_t i,uint64_t data);

uint64_t savm_ioctl_ram_read(savm_t* vm,uint64_t i);
void savm_ioctl_ram_write(savm_t* vm,uint64_t i,uint64_t data);

savm_error_e savm_create(savm_t* vm) {
	memset(vm,0,sizeof(savm_t));
	
	savm_error_e err = savm_mailbox_create(&vm->mailbox);
	if(err != SAVM_ERROR_NONE) return err;
	
	err = savm_rtc_create(&vm->rtc);
	if(err != SAVM_ERROR_NONE) return err;
	
	err = savm_uart_create(&vm->uart);
	if(err != SAVM_ERROR_NONE) return err;
	
	return savm_reset(vm);
}

savm_error_e savm_destroy(savm_t* vm) {
	for(size_t i = 0;i < SAVM_CPU_CORE_COUNT;i++) {
		if(vm->cpu.cores[i].irqs != NULL) free(vm->cpu.cores[i].irqs);
	}
	if(vm->io.ram != NULL) free(vm->io.ram);
	if(vm->io.mmap != NULL) free(vm->io.mmap);
	if(vm->io.pgdir != NULL) free(vm->io.pgdir);
	
	savm_error_e err = savm_mailbox_destroy(&vm->mailbox);
	if(err != SAVM_ERROR_NONE) return err;
	
	err = savm_rtc_destroy(&vm->rtc);
	if(err != SAVM_ERROR_NONE) return err;
	
	return savm_uart_destroy(&vm->uart);
}

savm_error_e savm_reset(savm_t* vm) {
	/* (Re)initialize the RAM */
	if(vm->io.ram != NULL) free(vm->io.ram);
	vm->io.ram = malloc(SAVM_IO_RAM_SIZE);
	if(vm->io.ram == NULL) return SAVM_ERROR_MEM;
	memset(vm->io.ram,0,SAVM_IO_RAM_SIZE);
	
	/* (Re)initialize the paging */
	if(vm->io.pgdir != NULL) free(vm->io.pgdir);
	
	/* (Re)initialize the memory map in the IO Controller */
	if(vm->io.mmap != NULL) free(vm->io.mmap);
	savm_error_e err = savm_ioctl_mmap(vm,SAVM_IO_RAM_BASE,SAVM_IO_RAM_END,savm_ioctl_ram_read,savm_ioctl_ram_write);
	if(err != SAVM_ERROR_NONE) return err;
	
	err = savm_ioctl_mmap(vm,SAVM_IO_IOCTL_BASE,SAVM_IO_IOCTL_END,savm_ioctl_ioctl_read,savm_ioctl_ioctl_write);
	if(err != SAVM_ERROR_NONE) return err;
	
	/* Reset the mailbox */
	err = savm_mailbox_reset(&vm->mailbox,vm);
	if(err != SAVM_ERROR_NONE) return err;
	
	/* Reset the RTC */
	err = savm_rtc_reset(&vm->rtc,vm);
	if(err != SAVM_ERROR_NONE) return err;
	
	/* Reset the UART */
	err = savm_uart_reset(&vm->uart,vm);
	if(err != SAVM_ERROR_NONE) return err;
	
	vm->cpu.currentCore = vm->io.selectedCore = 0;
	
	vm->cpu.cores[vm->cpu.currentCore].regs.pc = SAVM_IO_RAM_BASE;
	
	for(size_t i = 0;i < SAVM_CPU_CORE_COUNT;i++) {
		/* Reset the registers */
		vm->cpu.cores[i].regs.flags = SAVM_CPU_REG_FLAG_ENIRQ | SAVM_CPU_REG_FLAG_PRIV_KERN;
		vm->cpu.cores[i].regs.cycle = 0;
		
		/* Reset the CPU */
		memset(vm->cpu.cores[i].stack,0,sizeof(uint64_t)*SAVM_CPU_STACK_SIZE);
		memset(vm->cpu.cores[i].ivt,0,sizeof(uint64_t)*SAVM_CPU_IVT_SIZE);
		if(vm->cpu.cores[i].irqs != NULL) free(vm->cpu.cores[i].irqs);
		vm->cpu.cores[i].irqSize = 0;
		vm->cpu.cores[i].intr = 0;
		vm->cpu.cores[i].running = 0;
	}
	return SAVM_ERROR_NONE;
}

/* CPU */
savm_error_e savm_cpu_intr(savm_t* vm,uint64_t intr) {
	if(intr > SAVM_CPU_IVT_SIZE) return SAVM_ERROR_INVAL_INT;
	if(intr > SAVM_CPU_INT_BADPERM && vm->cpu.cores[vm->cpu.currentCore].regs.flags & SAVM_CPU_REG_FLAG_ENIRQ && vm->cpu.cores[vm->cpu.currentCore].regs.flags & SAVM_CPU_REG_FLAG_INTR) {
		size_t i = -1;
		if(vm->cpu.cores[vm->cpu.currentCore].irqs == NULL || vm->cpu.cores[vm->cpu.currentCore].irqSize < 1) {
			i = 0;
			vm->cpu.cores[vm->cpu.currentCore].irqSize = 1;
			vm->cpu.cores[vm->cpu.currentCore].irqs = malloc(sizeof(uint64_t));
		}
		for(size_t x = 0;x < vm->cpu.cores[vm->cpu.currentCore].irqSize;x++) {
			if(vm->cpu.cores[vm->cpu.currentCore].irqs[x] == -1) {
				i = x;
				break;
			}
		}
		if(i == -1) {
			i = vm->cpu.cores[vm->cpu.currentCore].irqSize++;
			vm->cpu.cores[vm->cpu.currentCore].irqs = realloc(vm->cpu.cores[vm->cpu.currentCore].irqs,sizeof(uint64_t)*vm->cpu.cores[vm->cpu.currentCore].irqSize);
		}
		if(vm->cpu.cores[vm->cpu.currentCore].irqs == NULL) return SAVM_ERROR_MEM;
		vm->cpu.cores[vm->cpu.currentCore].irqs[i] = intr;
		return SAVM_ERROR_NONE;
	}
	if(vm->cpu.cores[vm->cpu.currentCore].regs.flags & SAVM_CPU_REG_FLAG_INTR) {
		vm->cpu.cores[vm->cpu.currentCore].intr = SAVM_CPU_INT_FAULT;
		return SAVM_ERROR_NONE;
	}
	if(intr > SAVM_CPU_INT_BADPERM && !(vm->cpu.cores[vm->cpu.currentCore].regs.flags & SAVM_CPU_REG_FLAG_ENIRQ)) return SAVM_ERROR_NONE;
	memcpy(&vm->cpu.cores[vm->cpu.currentCore].iregs,&vm->cpu.cores[vm->cpu.currentCore].regs,sizeof(savm_cpu_regs_t));
	vm->cpu.cores[vm->cpu.currentCore].regs.flags |= SAVM_CPU_REG_FLAG_INTR & SAVM_CPU_REG_FLAG_PRIV_KERN;
	vm->cpu.cores[vm->cpu.currentCore].regs.flags &= ~SAVM_CPU_REG_FLAG_PRIV_USER;
	vm->cpu.cores[vm->cpu.currentCore].intr = intr;
	vm->cpu.cores[vm->cpu.currentCore].regs.pc = vm->cpu.cores[vm->cpu.currentCore].ivt[intr];
	return SAVM_ERROR_NONE;
}

savm_error_e savm_cpu_regread(savm_t* vm,uint64_t i,uint64_t* val) {
	switch(i) {
		case 0: /* flags */
			*val = vm->cpu.cores[vm->cpu.currentCore].regs.flags;
			break;
		case 1: /* tmp */
			*val = vm->cpu.cores[vm->cpu.currentCore].regs.tmp;
			break;
		case 2: /* sp */
			*val = vm->cpu.cores[vm->cpu.currentCore].regs.sp;
			break;
		case 3: /* ip */
			*val = vm->cpu.cores[vm->cpu.currentCore].regs.ip;
			break;
		case 4: /* pc */
			*val = vm->cpu.cores[vm->cpu.currentCore].regs.pc;
			break;
		case 5: /* cycle */
			*val = vm->cpu.cores[vm->cpu.currentCore].regs.cycle;
			break;
		case 6 ... 16: /* data */
			*val = vm->cpu.cores[vm->cpu.currentCore].regs.data[i-6];
			break;
		case 17 ... 27: /* index */
			*val = vm->cpu.cores[vm->cpu.currentCore].regs.index[i-17];
			break;
		case 28 ... 38: /* addr */
			*val = vm->cpu.cores[vm->cpu.currentCore].regs.addr[i-28];
			break;
		case 39 ... 49: /* ptr */
			*val = vm->cpu.cores[vm->cpu.currentCore].regs.ptr[i-39];
			break;
		default: return SAVM_ERROR_INVAL_ADDR;
	}
	return SAVM_ERROR_NONE;
}

savm_error_e savm_cpu_regwrite(savm_t* vm,uint64_t i,uint64_t val) {
	switch(i) {
		case 0: /* flags */
			if(val & SAVM_CPU_REG_FLAG_INTR && !(vm->cpu.cores[vm->cpu.currentCore].regs.flags & SAVM_CPU_REG_FLAG_INTR)) val &= ~SAVM_CPU_REG_FLAG_INTR;
			if(val & SAVM_CPU_REG_FLAG_PAGING && !(vm->cpu.cores[vm->cpu.currentCore].regs.flags & SAVM_CPU_REG_FLAG_PAGING)) val &= ~SAVM_CPU_REG_FLAG_PAGING;
			if(val & SAVM_CPU_REG_FLAG_PRIV_KERN && !(vm->cpu.cores[vm->cpu.currentCore].regs.flags & SAVM_CPU_REG_FLAG_PRIV_KERN)) val &= ~SAVM_CPU_REG_FLAG_PRIV_KERN;
			if(val & SAVM_CPU_REG_FLAG_PRIV_USER && !(vm->cpu.cores[vm->cpu.currentCore].regs.flags & SAVM_CPU_REG_FLAG_PRIV_USER)) val &= ~SAVM_CPU_REG_FLAG_PRIV_USER;
			
			if(!(vm->cpu.cores[vm->cpu.currentCore].regs.flags & SAVM_CPU_REG_FLAG_PAGING) && val & SAVM_CPU_REG_FLAG_PAGING) {
				if(vm->io.pgdir == NULL) {
					savm_error_e err = savm_cpu_intr(vm,SAVM_CPU_INT_FAULT);
					if(err != SAVM_ERROR_NONE) return err;
				}
			}
			vm->cpu.cores[vm->cpu.currentCore].regs.flags = val;
			break;
		case 1: /* tmp */
			vm->cpu.cores[vm->cpu.currentCore].regs.tmp = val;
			break;
		case 2: /* sp */
		case 3: /* ip */
		case 4: /* pc */
		case 5: /* cycle */
			return SAVM_ERROR_INVAL_ADDR;
		case 6 ... 16: /* data */
			return vm->cpu.cores[vm->cpu.currentCore].regs.data[i-6];
		case 17 ... 27: /* index */
			return vm->cpu.cores[vm->cpu.currentCore].regs.index[i-17];
		case 28 ... 38: /* addr */
			return vm->cpu.cores[vm->cpu.currentCore].regs.addr[i-28];
		case 39 ... 49: /* ptr */
			return vm->cpu.cores[vm->cpu.currentCore].regs.ptr[i-39];
		default: return SAVM_ERROR_INVAL_ADDR;
	}
	return SAVM_ERROR_NONE;
}

savm_error_e savm_cpu_cycle_core(savm_t* vm,uint8_t core) {
	/* Load the current core */
	if(core >= SAVM_CPU_CORE_COUNT) return SAVM_ERROR_INVAL_ADDR;
	
	vm->cpu.currentCore = core;

	/* Temporary patch for issue #1 */
	if(vm->cpu.cores[vm->cpu.currentCore].regs.pc == 0) vm->cpu.cores[vm->cpu.currentCore].regs.pc = SAVM_IO_RAM_BASE+(vm->cpu.cores[vm->cpu.currentCore].regs.cycle*3);
	
	/* Fetches the instruction from memory */
	savm_error_e err = savm_ioctl_read(vm,vm->cpu.cores[vm->cpu.currentCore].regs.pc,&vm->cpu.cores[vm->cpu.currentCore].regs.ip);
	if(err != SAVM_ERROR_NONE) return err;
	
	/* Decodes the instruction */
	uint64_t addr;
	uint64_t val;
	
	err = savm_ioctl_read(vm,vm->cpu.cores[vm->cpu.currentCore].regs.pc+1,&addr);
	if(err != SAVM_ERROR_NONE) return err;
	
	err = savm_ioctl_read(vm,vm->cpu.cores[vm->cpu.currentCore].regs.pc+2,&val);
	if(err != SAVM_ERROR_NONE) return err;
	
	vm->cpu.cores[vm->cpu.currentCore].regs.pc += 3;
	
	/* Execute the instruction */
	switch(vm->cpu.cores[vm->cpu.currentCore].regs.ip) {
		case 0: /* NOP */
			vm->cpu.cores[vm->cpu.currentCore].running = 0;
			break;
		case 1: /* ADDR */
			{
				uint64_t a;
				err = savm_cpu_regread(vm,addr,&a);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				uint64_t b;
				err = savm_cpu_regread(vm,val,&b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
				err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				err = savm_cpu_regwrite(vm,addr,a+b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			}
			break;
		case 2: /* ADDM */
			{
				uint64_t a;
				err = savm_ioctl_read(vm,addr,&a);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				uint64_t b;
				err = savm_ioctl_read(vm,val,&b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				err = savm_ioctl_write(vm,addr,a+b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			}
			break;
		case 3: /* SUBR */
			{
				uint64_t a;
				err = savm_cpu_regread(vm,addr,&a);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				uint64_t b;
				err = savm_cpu_regread(vm,val,&b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				err = savm_cpu_regwrite(vm,addr,a-b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			}
			break;
		case 4: /* SUBM */
			{
				uint64_t a;
				err = savm_ioctl_read(vm,addr,&a);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				uint64_t b;
				err = savm_ioctl_read(vm,val,&b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				err = savm_ioctl_write(vm,addr,a-b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			}
			break;
		case 5: /* MULR */
			{
				uint64_t a;
				err = savm_cpu_regread(vm,addr,&a);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				uint64_t b;
				err = savm_cpu_regread(vm,val,&b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				err = savm_cpu_regwrite(vm,addr,a*b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			}
			break;
		case 6: /* MULM */
			{
				uint64_t a;
				err = savm_ioctl_read(vm,addr,&a);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				uint64_t b;
				err = savm_ioctl_read(vm,val,&b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				err = savm_ioctl_write(vm,addr,a*b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			}
			break;
		case 7: /* DIVR */
			{
				uint64_t a;
				err = savm_cpu_regread(vm,addr,&a);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				uint64_t b;
				err = savm_cpu_regread(vm,val,&b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				if(b == 0) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_DIVBYZERO);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				err = savm_cpu_regwrite(vm,addr,a/b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			}
			break;
		case 8: /* DIVM */
			{
				uint64_t a;
				err = savm_ioctl_read(vm,addr,&a);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				uint64_t b;
				err = savm_ioctl_read(vm,val,&b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				if(b == 0) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_DIVBYZERO);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				err = savm_ioctl_write(vm,addr,a/b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			}
			break;
		case 9: /* ANDR */
			{
				uint64_t a;
				err = savm_cpu_regread(vm,addr,&a);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				uint64_t b;
				err = savm_cpu_regread(vm,val,&b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				err = savm_cpu_regwrite(vm,addr,a & b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			}
			break;
		case 10: /* ANDM */
			{
				uint64_t a;
				err = savm_ioctl_read(vm,addr,&a);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				uint64_t b;
				err = savm_ioctl_read(vm,val,&b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				err = savm_ioctl_write(vm,addr,a & b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			}
			break;
		case 11: /* ORR */
			{
				uint64_t a;
				err = savm_cpu_regread(vm,addr,&a);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				uint64_t b;
				err = savm_cpu_regread(vm,val,&b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				err = savm_cpu_regwrite(vm,addr,a | b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			}
			break;
		case 12: /* ORM */
			{
				uint64_t a;
				err = savm_ioctl_read(vm,addr,&a);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				uint64_t b;
				err = savm_ioctl_read(vm,val,&b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				err = savm_ioctl_write(vm,addr,a | b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			}
			break;
		case 13: /* XORR */
			{
				uint64_t a;
				err = savm_cpu_regread(vm,addr,&a);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				uint64_t b;
				err = savm_cpu_regread(vm,val,&b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				err = savm_cpu_regwrite(vm,addr,a ^ b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			}
			break;
		case 14: /* XORM */
			{
				uint64_t a;
				err = savm_ioctl_read(vm,addr,&a);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				uint64_t b;
				err = savm_ioctl_read(vm,val,&b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				err = savm_ioctl_write(vm,addr,a ^ b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			}
			break;
		case 15: /* NORR */
			{
				uint64_t a;
				err = savm_cpu_regread(vm,addr,&a);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				uint64_t b;
				err = savm_cpu_regread(vm,val,&b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				err = savm_cpu_regwrite(vm,addr,~(a | b));
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			}
			break;
		case 16: /* NORM */
			{
				uint64_t a;
				err = savm_ioctl_read(vm,addr,&a);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				};
				
				uint64_t b;
				err = savm_ioctl_read(vm,val,&b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				err = savm_ioctl_write(vm,addr,~(a | b));
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			}
			break;
		case 17: /* NANDR */
			{
				uint64_t a;
				err = savm_cpu_regread(vm,addr,&a);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				uint64_t b;
				err = savm_cpu_regread(vm,val,&b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				err = savm_cpu_regwrite(vm,addr,~(a & b));
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			}
			break;
		case 18: /* NANDM */
			{
				uint64_t a;
				err = savm_ioctl_read(vm,addr,&a);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				uint64_t b;
				err = savm_ioctl_read(vm,val,&b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				err = savm_ioctl_write(vm,addr,~(a & b));
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			}
			break;
		case 19: /* LSHIFTR */
			{
				uint64_t a;
				err = savm_cpu_regread(vm,addr,&a);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				uint64_t b;
				err = savm_cpu_regread(vm,val,&b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				err = savm_cpu_regwrite(vm,addr,a << b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			}
			break;
		case 20: /* LSHIFTM */
			{
				uint64_t a;
				err = savm_ioctl_read(vm,addr,&a);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				uint64_t b;
				err = savm_ioctl_read(vm,val,&b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				err = savm_ioctl_write(vm,addr,a << b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			}
			break;
		case 21: /* RSHIFTR */
			{
				uint64_t a;
				err = savm_cpu_regread(vm,addr,&a);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				uint64_t b;
				err = savm_cpu_regread(vm,val,&b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				err = savm_cpu_regwrite(vm,addr,a >> b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			}
			break;
		case 22: /* RSHIFTM */
			{
				uint64_t a;
				err = savm_ioctl_read(vm,addr,&a);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				uint64_t b;
				err = savm_ioctl_read(vm,val,&b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				err = savm_ioctl_write(vm,addr,a >> b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			}
			break;
		case 23: /* CMPR */
			{
				uint64_t a;
				err = savm_cpu_regread(vm,addr,&a);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				uint64_t b;
				err = savm_cpu_regread(vm,val,&b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				vm->cpu.cores[vm->cpu.currentCore].regs.tmp = a == b;
			}
			break;
		case 24: /* CMPM */
			{
				uint64_t a;
				err = savm_ioctl_read(vm,addr,&a);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				uint64_t b;
				err = savm_ioctl_read(vm,val,&b);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				vm->cpu.cores[vm->cpu.currentCore].regs.tmp = a == b;
			}
			break;
		case 25: /* JITR */
			{
				uint64_t a;
				err = savm_cpu_regread(vm,addr,&a);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				if(vm->cpu.cores[vm->cpu.currentCore].regs.tmp) vm->cpu.cores[vm->cpu.currentCore].regs.pc = a;
			}
			break;
		case 26: /* JITM */
			{
				uint64_t a;
				err = savm_ioctl_read(vm,addr,&a);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				if(vm->cpu.cores[vm->cpu.currentCore].regs.tmp) vm->cpu.cores[vm->cpu.currentCore].regs.pc = a;
			}
			break;
		case 27: /* JIT */
			if(vm->cpu.cores[vm->cpu.currentCore].regs.tmp) vm->cpu.cores[vm->cpu.currentCore].regs.pc = addr;
			break;
		case 28: /* JMPR */
			{
				uint64_t a;
				err = savm_cpu_regread(vm,addr,&vm->cpu.cores[vm->cpu.currentCore].regs.pc);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			}
			break;
		case 29: /* JMPM */
			{
				uint64_t a;
				err = savm_ioctl_read(vm,addr,&vm->cpu.cores[vm->cpu.currentCore].regs.pc);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			}
			break;
		case 30: /* JMP */
			vm->cpu.cores[vm->cpu.currentCore].regs.pc = addr;
			break;
		case 31: /* CALLR */
			if(vm->cpu.cores[vm->cpu.currentCore].regs.sp < SAVM_CPU_STACK_SIZE) {
				vm->cpu.cores[vm->cpu.currentCore].stack[vm->cpu.cores[vm->cpu.currentCore].regs.sp++] = vm->cpu.cores[vm->cpu.currentCore].regs.pc;
				err = savm_cpu_regread(vm,addr,&vm->cpu.cores[vm->cpu.currentCore].regs.pc);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			} else {
				err = savm_cpu_intr(vm,SAVM_CPU_INT_STACK_OVERFLOW);
				if(err != SAVM_ERROR_NONE) return err;
			}
			break;
		case 32: /* CALLM */
			if(vm->cpu.cores[vm->cpu.currentCore].regs.sp < SAVM_CPU_STACK_SIZE) {
				vm->cpu.cores[vm->cpu.currentCore].stack[vm->cpu.cores[vm->cpu.currentCore].regs.sp++] = vm->cpu.cores[vm->cpu.currentCore].regs.pc;
				err = savm_ioctl_read(vm,addr,&vm->cpu.cores[vm->cpu.currentCore].regs.pc);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			} else {
				err = savm_cpu_intr(vm,SAVM_CPU_INT_STACK_OVERFLOW);
				if(err != SAVM_ERROR_NONE) return err;
			}
			break;
		case 33: /* CALL */
			if(vm->cpu.cores[vm->cpu.currentCore].regs.sp < SAVM_CPU_STACK_SIZE) {
				vm->cpu.cores[vm->cpu.currentCore].stack[vm->cpu.cores[vm->cpu.currentCore].regs.sp++] = vm->cpu.cores[vm->cpu.currentCore].regs.pc;
				vm->cpu.cores[vm->cpu.currentCore].regs.pc = addr;
			} else {
				err = savm_cpu_intr(vm,SAVM_CPU_INT_STACK_OVERFLOW);
				if(err != SAVM_ERROR_NONE) return err;
			}
			break;
		case 34: /* RET */
			if(vm->cpu.cores[vm->cpu.currentCore].regs.sp < SAVM_CPU_STACK_SIZE) vm->cpu.cores[vm->cpu.currentCore].regs.pc = vm->cpu.cores[vm->cpu.currentCore].stack[vm->cpu.cores[vm->cpu.currentCore].regs.sp--];
			else {
				err = savm_cpu_intr(vm,SAVM_CPU_INT_STACK_OVERFLOW);
				if(err != SAVM_ERROR_NONE) return err;
			}
			break;
		case 35: /* PUSHR */
			if(vm->cpu.cores[vm->cpu.currentCore].regs.sp < SAVM_CPU_STACK_SIZE) {
				err = savm_cpu_regread(vm,addr,&vm->cpu.cores[vm->cpu.currentCore].stack[vm->cpu.cores[vm->cpu.currentCore].regs.sp++]);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			} else {
				err = savm_cpu_intr(vm,SAVM_CPU_INT_STACK_OVERFLOW);
				if(err != SAVM_ERROR_NONE) return err;
			}
			break;
		case 36: /* PUSHM */
			if(vm->cpu.cores[vm->cpu.currentCore].regs.sp < SAVM_CPU_STACK_SIZE) {
				err = savm_ioctl_read(vm,addr,&vm->cpu.cores[vm->cpu.currentCore].stack[vm->cpu.cores[vm->cpu.currentCore].regs.sp++]);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			} else {
				err = savm_cpu_intr(vm,SAVM_CPU_INT_STACK_OVERFLOW);
				if(err != SAVM_ERROR_NONE) return err;
			}
			break;
		case 37: /* POPR */
			if(vm->cpu.cores[vm->cpu.currentCore].regs.sp < SAVM_CPU_STACK_SIZE) {
				err = savm_cpu_regwrite(vm,addr,vm->cpu.cores[vm->cpu.currentCore].stack[vm->cpu.cores[vm->cpu.currentCore].regs.sp--]);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			} else {
				err = savm_cpu_intr(vm,SAVM_CPU_INT_STACK_OVERFLOW);
				if(err != SAVM_ERROR_NONE) return err;
			}
			break;
		case 38: /* POPM */
			if(vm->cpu.cores[vm->cpu.currentCore].regs.sp < SAVM_CPU_STACK_SIZE) {
				err = savm_ioctl_write(vm,addr,vm->cpu.cores[vm->cpu.currentCore].stack[vm->cpu.cores[vm->cpu.currentCore].regs.sp--]);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			} else {
				err = savm_cpu_intr(vm,SAVM_CPU_INT_STACK_OVERFLOW);
				if(err != SAVM_ERROR_NONE) return err;
			}
			break;
		case 39: /* MOVRR */
			{
				err = savm_cpu_regread(vm,val,&val);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				err = savm_cpu_regwrite(vm,addr,val);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			}
			break;
		case 40: /* MOVRM */
			{
				err = savm_cpu_regread(vm,val,&val);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				err = savm_ioctl_write(vm,addr,val);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			}
			break;
		case 41: /* MOVMR */
			{
				err = savm_ioctl_read(vm,val,&val);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				err = savm_cpu_regwrite(vm,addr,val);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			}
			break;
		case 42: /* MOVMM */
			{
				err = savm_ioctl_read(vm,val,&val);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				err = savm_ioctl_write(vm,addr,val);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			}
			break;
		case 43: /* STOR */
			{
				err = savm_cpu_regwrite(vm,addr,val);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			}
		case 44: /* STOM */
			{
				err = savm_ioctl_write(vm,addr,val);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			}
			break;
		case 45: /* INTR */
			if(vm->cpu.cores[vm->cpu.currentCore].regs.flags & SAVM_CPU_REG_FLAG_PRIV_KERN) {
				err = savm_cpu_regread(vm,addr,&val);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				err = savm_cpu_intr(vm,val);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_INT) return err;
				if(err == SAVM_ERROR_INVAL_INT) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
				}
			} else {
				err = savm_cpu_intr(vm,SAVM_CPU_INT_BADPERM);
				if(err != SAVM_ERROR_NONE) return err;
			}
			break;
		case 46: /* INTM */
			if(vm->cpu.cores[vm->cpu.currentCore].regs.flags & SAVM_CPU_REG_FLAG_PRIV_KERN) {
				err = savm_ioctl_read(vm,addr,&val);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err == SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				err = savm_cpu_intr(vm,val);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_INT) return err;
				if(err == SAVM_ERROR_INVAL_INT) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
				}
			} else {
				err = savm_cpu_intr(vm,SAVM_CPU_INT_BADPERM);
				if(err != SAVM_ERROR_NONE) return err;
			}
			break;
		case 47: /* INT */
			if(vm->cpu.cores[vm->cpu.currentCore].regs.flags & SAVM_CPU_REG_FLAG_PRIV_KERN) {
				err = savm_cpu_intr(vm,addr);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_INT) return err;
				if(err == SAVM_ERROR_INVAL_INT) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
				}
			} else {
				err = savm_cpu_intr(vm,SAVM_CPU_INT_BADPERM);
				if(err != SAVM_ERROR_NONE) return err;
			}
			break;
		case 48: /* IRET */
			if(vm->cpu.cores[vm->cpu.currentCore].regs.flags & SAVM_CPU_REG_FLAG_INTR || vm->cpu.cores[vm->cpu.currentCore].regs.flags & SAVM_CPU_REG_FLAG_PRIV_KERN) {
				memcpy(&vm->cpu.cores[vm->cpu.currentCore].regs,&vm->cpu.cores[vm->cpu.currentCore].iregs,sizeof(savm_cpu_regs_t));
			} else {
				err = savm_cpu_intr(vm,SAVM_CPU_INT_FAULT);
				if(err != SAVM_ERROR_NONE) return err;
			}
		case 49: /* LDITBLR */
			if(vm->cpu.cores[vm->cpu.currentCore].regs.flags & SAVM_CPU_REG_FLAG_PRIV_KERN) {
				err = savm_cpu_regread(vm,addr,&addr);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				for(uint64_t i = 0;i < SAVM_CPU_IVT_SIZE;i++) {
					err = savm_ioctl_read(vm,addr+i,&vm->cpu.cores[vm->cpu.currentCore].ivt[i]);
					if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
					if(err == SAVM_ERROR_INVAL_ADDR || err == SAVM_ERROR_NOTMAPPED) {
						err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
						if(err != SAVM_ERROR_NONE) return err;
						break;
					}
				}
			} else {
				err = savm_cpu_intr(vm,SAVM_CPU_INT_BADPERM);
				if(err != SAVM_ERROR_NONE) return err;
			}
			break;
		case 50: /* LDITBLM */
			if(vm->cpu.cores[vm->cpu.currentCore].regs.flags & SAVM_CPU_REG_FLAG_PRIV_KERN) {
				err = savm_ioctl_read(vm,addr,&addr);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
					if(err == SAVM_ERROR_INVAL_ADDR || err == SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				for(uint64_t i = 0;i < SAVM_CPU_IVT_SIZE;i++) {
					err = savm_ioctl_read(vm,addr+i,&vm->cpu.cores[vm->cpu.currentCore].ivt[i]);
					if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
					if(err == SAVM_ERROR_INVAL_ADDR || err == SAVM_ERROR_NOTMAPPED) {
						err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
						if(err != SAVM_ERROR_NONE) return err;
						break;
					}
				}
			} else {
				err = savm_cpu_intr(vm,SAVM_CPU_INT_BADPERM);
				if(err != SAVM_ERROR_NONE) return err;
			}
			break;
		case 51: /* HLT */
			if(vm->cpu.cores[vm->cpu.currentCore].regs.flags & SAVM_CPU_REG_FLAG_PRIV_KERN) {
				vm->cpu.cores[vm->cpu.currentCore].running = 0;
			} else {
				err = savm_cpu_intr(vm,SAVM_CPU_INT_BADPERM);
				if(err != SAVM_ERROR_NONE) return err;
			}
			break;
		case 52: /* RST */
			if(vm->cpu.cores[vm->cpu.currentCore].regs.flags & SAVM_CPU_REG_FLAG_PRIV_KERN) {
				err = savm_reset(vm);
				if(err != SAVM_ERROR_NONE) return err;
				vm->cpu.cores[vm->cpu.currentCore].running = 1;
				vm->cpu.cores[vm->cpu.currentCore].regs.cycle = -1;
			} else {
				err = savm_cpu_intr(vm,SAVM_CPU_INT_BADPERM);
				if(err != SAVM_ERROR_NONE) return err;
			}
		default:
			err = savm_cpu_intr(vm,SAVM_CPU_INT_BADINSTR);
			if(err != SAVM_CPU_INT_BADINSTR) return err;
			break;
	}
	
	err = savm_rtc_cycle(&vm->rtc,vm);
	if(err != SAVM_ERROR_NONE) return err;
	
	err = savm_mailbox_cycle(&vm->mailbox,vm);
	if(err != SAVM_ERROR_NONE) return err;
	
	err = savm_uart_cycle(&vm->uart,vm);
	if(err != SAVM_ERROR_NONE) return err;
	
	if(vm->cpu.cores[vm->cpu.currentCore].regs.flags & SAVM_CPU_REG_FLAG_INTR && vm->cpu.cores[vm->cpu.currentCore].regs.flags & SAVM_CPU_REG_FLAG_ENIRQ && vm->cpu.cores[vm->cpu.currentCore].irqSize > 0) {
		uint64_t irq = vm->cpu.cores[vm->cpu.currentCore].irqs[0];
		vm->cpu.cores[vm->cpu.currentCore].irqs[0] = -1;
		err = savm_cpu_intr(vm,irq);
		if(err != SAVM_ERROR_NONE) return err;
	}
	
	vm->cpu.cores[vm->cpu.currentCore].regs.cycle++;
	return SAVM_ERROR_NONE;
}

savm_error_e savm_cpu_cycle(savm_t* vm) {
	for(size_t i = 0;i < SAVM_CPU_CORE_COUNT;i++) {
		if(vm->cpu.cores[i].running) {
			savm_error_e err = savm_cpu_cycle_core(vm,i);
			if(err != SAVM_ERROR_NONE) return err;
		}
	}
	return SAVM_ERROR_NONE;
}

/* IO Controller */
savm_error_e savm_ioctl_loadfile(savm_t* vm,uint64_t addr,char* path) {
	FILE* fp = fopen(path,"rb");
	if(fp == NULL) return SAVM_ERROR_INVAL_FILE;
	struct stat st;
	stat(path,&st);
	savm_error_e err = savm_ioctl_loadfp(vm,addr,(st.st_size+1)/sizeof(uint64_t),fp);
	fclose(fp);
	return err;
}

savm_error_e savm_ioctl_loadfp(savm_t* vm,uint64_t addr,uint64_t size,FILE* fp) {
	uint64_t* data = malloc(sizeof(uint64_t)*size);
	if(data == NULL) return SAVM_ERROR_MEM;
	memset(data,0,sizeof(uint64_t)*size);
	fread(data,sizeof(uint64_t)*size,1,fp);
	for(uint64_t i = 0;i < size;i++) {
		savm_error_e err = savm_ioctl_write(vm,addr+i,data[i]);
		if(err != SAVM_ERROR_NONE) {
			free(data);
			return err;
		}
	}
	free(data);
	return SAVM_ERROR_NONE;
}

savm_error_e savm_ioctl_dumpfile(savm_t* vm,uint64_t addr,uint64_t size,char* path) {
	FILE* fp = fopen(path,"w");
	if(fp == NULL) return SAVM_ERROR_INVAL_FILE;
	savm_error_e err = savm_ioctl_dumpfp(vm,addr,size,fp);
	fclose(fp);
	return err;
}

savm_error_e savm_ioctl_dumpfp(savm_t* vm,uint64_t addr,uint64_t size,FILE* fp) {
	uint64_t* data = malloc(sizeof(uint64_t)*size);
	if(data == NULL) return SAVM_ERROR_MEM;
	memset(data,0,sizeof(uint64_t)*size);
	
	for(uint64_t i = 0;i < size;i++) {
		savm_error_e err = savm_ioctl_read(vm,addr+i,&data[i]);
		if(err != SAVM_ERROR_NONE) {
			free(data);
			return err;
		}
	}
	
	fwrite(data,sizeof(uint64_t)*size,1,fp);
	
	free(data);
	return SAVM_ERROR_NONE;
}

savm_error_e savm_ioctl_mmap(savm_t* vm,uint64_t addr,uint64_t end,savm_ioctl_read_p read,savm_ioctl_write_p write) {
	for(size_t x = 0;x < vm->io.mmapSize;x++) {
		if(vm->io.mmap[x].addr == addr && vm->io.mmap[x].end == end) return SAVM_ERROR_MAPPED;
	}
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
	if(vm->cpu.cores[vm->cpu.currentCore].regs.flags & SAVM_CPU_REG_FLAG_PAGING) {
		for(uint64_t t = 0;t < vm->io.pgdir->tableCount;t++) {
			for(uint64_t p = 0;p < vm->io.pgdir->tables[t].size;p++) {
				if(vm->io.pgdir->tables[t].page[p].address <= addr && vm->io.pgdir->tables[t].page[p].address+vm->io.pgdir->tables[t].page[p].size > addr) {
					if(vm->cpu.cores[vm->cpu.currentCore].regs.flags & SAVM_CPU_REG_FLAG_PRIV_KERN && !(vm->io.pgdir->tables[t].page[p].perms & SAVM_CPU_PAGING_PERM_KERN)) {
						savm_error_e err = savm_cpu_intr(vm,SAVM_CPU_INT_BADPERM);
						if(err != SAVM_ERROR_NONE) return err;
					}
					if(vm->cpu.cores[vm->cpu.currentCore].regs.flags & SAVM_CPU_REG_FLAG_PRIV_USER && !(vm->io.pgdir->tables[t].page[p].perms & SAVM_CPU_PAGING_PERM_USER)) {
						savm_error_e err = savm_cpu_intr(vm,SAVM_CPU_INT_BADPERM);
						if(err != SAVM_ERROR_NONE) return err;
					}
				}
			}
		}
	}
	for(size_t i = 0;i < vm->io.mmapSize;i++) {
		if(vm->io.mmap[i].addr <= addr && vm->io.mmap[i].end > addr) {
			*data = vm->io.mmap[i].read(vm,addr-vm->io.mmap[i].addr);
			return SAVM_ERROR_NONE;
		}
	}
	return SAVM_ERROR_NOTMAPPED;
}

savm_error_e savm_ioctl_write(savm_t* vm,uint64_t addr,uint64_t data) {
	if(vm->io.mmap == NULL) return SAVM_ERROR_NOTMAPPED;
	if(vm->cpu.cores[vm->cpu.currentCore].regs.flags & SAVM_CPU_REG_FLAG_PAGING) {
		for(uint64_t t = 0;t < vm->io.pgdir->tableCount;t++) {
			for(uint64_t p = 0;p < vm->io.pgdir->tables[t].size;p++) {
				if(vm->io.pgdir->tables[t].page[p].address <= addr && vm->io.pgdir->tables[t].page[p].address+vm->io.pgdir->tables[t].page[p].size > addr) {
					if(vm->cpu.cores[vm->cpu.currentCore].regs.flags & SAVM_CPU_REG_FLAG_PRIV_KERN && !(vm->io.pgdir->tables[t].page[p].perms & SAVM_CPU_PAGING_PERM_KERN)) {
						savm_error_e err = savm_cpu_intr(vm,SAVM_CPU_INT_BADPERM);
						if(err != SAVM_ERROR_NONE) return err;
					}
					if(vm->cpu.cores[vm->cpu.currentCore].regs.flags & SAVM_CPU_REG_FLAG_PRIV_USER && !(vm->io.pgdir->tables[t].page[p].perms & SAVM_CPU_PAGING_PERM_USER)) {
						savm_error_e err = savm_cpu_intr(vm,SAVM_CPU_INT_BADPERM);
						if(err != SAVM_ERROR_NONE) return err;
					}
				}
			}
		}
	}
	for(size_t i = 0;i < vm->io.mmapSize;i++) {
		if(vm->io.mmap[i].addr <= addr && vm->io.mmap[i].end > addr) {
			vm->io.mmap[i].write(vm,addr-vm->io.mmap[i].addr,data);
			return SAVM_ERROR_NONE;
		}
	}
	return SAVM_ERROR_NOTMAPPED;
}

uint64_t savm_ioctl_ioctl_read(savm_t* vm,uint64_t i) {
	switch(i) {
		case 0: return SAVM_IO_RAM_SIZE;
		case 2: return SAVM_CPU_CORE_COUNT;
		case 3: return vm->cpu.currentCore;
		case 4: return vm->io.selectedCore;
	}
	return 0;
}

void savm_ioctl_ioctl_write(savm_t* vm,uint64_t i,uint64_t data) {
	switch(i) {
		case 1:
			if(vm->cpu.cores[vm->cpu.currentCore].regs.flags & SAVM_CPU_REG_FLAG_PRIV_USER) {
				savm_error_e err = savm_cpu_intr(vm,SAVM_CPU_INT_BADPERM);
				if(err != SAVM_ERROR_NONE) {
					vm->cpu.cores[vm->cpu.currentCore].running = 0;
					return;
				}
			}
			vm->io.pgdir = malloc(sizeof(savm_pagedir_t));
			savm_ioctl_read(vm,data,&vm->io.pgdir->tableCount);
			vm->io.pgdir->tables = malloc(sizeof(savm_pagetbl_t)*vm->io.pgdir->tableCount);
			for(uint64_t i = 0;i < vm->io.pgdir->tableCount;i++) {
				savm_ioctl_read(vm,data,&vm->io.pgdir->tables[i].size);
				vm->io.pgdir->tables[i].page = malloc(sizeof(savm_page_t)*vm->io.pgdir->tables[i].size);
				for(uint64_t x = 0;x < vm->io.pgdir->tables[i].size;x++) {
					uint64_t tmp;
					savm_ioctl_read(vm,data,&tmp);
					vm->io.pgdir->tables[i].page[x].flags = tmp & 0xffffffff;
					vm->io.pgdir->tables[i].page[x].perms = tmp >> 32;
					
					savm_ioctl_read(vm,data+1,&vm->io.pgdir->tables[i].page[x].size);
					savm_ioctl_read(vm,data+2,&vm->io.pgdir->tables[i].page[x].address);
					
					data += 3;
				}
				data += 1;
			}
			break;
		case 2:
			if(data > SAVM_CPU_CORE_COUNT) {
				savm_error_e err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
				if(err != SAVM_ERROR_NONE) {
					vm->cpu.cores[vm->cpu.currentCore].running = 0;
					return;
				}
				break;
			}
			vm->io.selectedCore = 1;
			break;
		case 3:
			if(vm->io.selectedCore > SAVM_CPU_CORE_COUNT) {
				savm_error_e err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
				if(err != SAVM_ERROR_NONE) {
					vm->cpu.cores[vm->cpu.currentCore].running = 0;
					return;
				}
				break;
			}
			vm->cpu.cores[vm->io.selectedCore].running = data;
			break;
		case 4:
			if(vm->io.selectedCore > SAVM_CPU_CORE_COUNT) {
				savm_error_e err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
				if(err != SAVM_ERROR_NONE) {
					vm->cpu.cores[vm->cpu.currentCore].running = 0;
					return;
				}
				break;
			}
			vm->cpu.cores[vm->io.selectedCore].regs.pc = data;
			break;
	}
}

uint64_t savm_ioctl_ram_read(savm_t* vm,uint64_t i) {
	return vm->io.ram[i];
}

void savm_ioctl_ram_write(savm_t* vm,uint64_t i,uint64_t data) {
	vm->io.ram[i] = data;
}
