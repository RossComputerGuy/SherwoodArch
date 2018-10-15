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
	if(vm->cpu.irqs != NULL) free(vm->cpu.irqs);
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
	
	/* Reset the registers */
	vm->cpu.regs.flags = SAVM_CPU_REG_FLAG_ENIRQ | SAVM_CPU_REG_FLAG_PRIV_KERN;
	vm->cpu.regs.pc = SAVM_IO_RAM_BASE;
	vm->cpu.regs.cycle = 0;
	
	/* Reset the CPU */
	memset(vm->cpu.stack,0,sizeof(uint64_t)*SAVM_CPU_STACK_SIZE);
	memset(vm->cpu.ivt,0,sizeof(uint64_t)*SAVM_CPU_IVT_SIZE);
	if(vm->cpu.irqs != NULL) free(vm->cpu.irqs);
	vm->cpu.irqSize = 0;
	vm->cpu.intr = 0;
	vm->cpu.running = 0;
	return SAVM_ERROR_NONE;
}

/* CPU */
savm_error_e savm_cpu_intr(savm_t* vm,uint64_t intr) {
	if(intr > SAVM_CPU_IVT_SIZE) return SAVM_ERROR_INVAL_INT;
	if(intr > SAVM_CPU_INT_BADPERM && vm->cpu.regs.flags & SAVM_CPU_REG_FLAG_ENIRQ && vm->cpu.regs.flags & SAVM_CPU_REG_FLAG_INTR) {
		size_t i = -1;
		if(vm->cpu.irqs == NULL || vm->cpu.irqSize < 1) {
			i = 0;
			vm->cpu.irqSize = 1;
			vm->cpu.irqs = malloc(sizeof(uint64_t));
		}
		for(size_t x = 0;x < vm->cpu.irqSize;x++) {
			if(vm->cpu.irqs[x] == -1) {
				i = x;
				break;
			}
		}
		if(i == -1) {
			i = vm->cpu.irqSize++;
			vm->cpu.irqs = realloc(vm->cpu.irqs,sizeof(uint64_t)*vm->cpu.irqSize);
		}
		if(vm->cpu.irqs == NULL) return SAVM_ERROR_MEM;
		vm->cpu.irqs[i] = intr;
		return SAVM_ERROR_NONE;
	}
	if(vm->cpu.regs.flags & SAVM_CPU_REG_FLAG_INTR) {
		vm->cpu.intr = SAVM_CPU_INT_FAULT;
		return SAVM_ERROR_NONE;
	}
	if(intr > SAVM_CPU_INT_BADPERM && !(vm->cpu.regs.flags & SAVM_CPU_REG_FLAG_ENIRQ)) return SAVM_ERROR_NONE;
	memcpy(&vm->cpu.iregs,&vm->cpu.regs,sizeof(savm_cpu_regs_t));
	vm->cpu.regs.flags |= SAVM_CPU_REG_FLAG_INTR & SAVM_CPU_REG_FLAG_PRIV_KERN;
	vm->cpu.regs.flags &= ~SAVM_CPU_REG_FLAG_PRIV_USER;
	vm->cpu.intr = intr;
	vm->cpu.regs.pc = vm->cpu.ivt[intr];
	return SAVM_ERROR_NONE;
}

savm_error_e savm_cpu_regread(savm_t* vm,uint64_t i,uint64_t* val) {
	switch(i) {
		case 0: /* flags */
			*val = vm->cpu.regs.flags;
			break;
		case 1: /* tmp */
			*val = vm->cpu.regs.tmp;
			break;
		case 2: /* sp */
			*val = vm->cpu.regs.sp;
			break;
		case 3: /* ip */
			*val = vm->cpu.regs.ip;
			break;
		case 4: /* pc */
			*val = vm->cpu.regs.pc;
			break;
		case 5: /* cycle */
			*val = vm->cpu.regs.cycle;
			break;
		case 6 ... 16: /* data */
			*val = vm->cpu.regs.data[i-6];
			break;
		case 17 ... 27: /* index */
			*val = vm->cpu.regs.index[i-17];
			break;
		case 28 ... 38: /* addr */
			*val = vm->cpu.regs.addr[i-28];
			break;
		case 39 ... 49: /* ptr */
			*val = vm->cpu.regs.ptr[i-39];
			break;
		default: return SAVM_ERROR_INVAL_ADDR;
	}
	return SAVM_ERROR_NONE;
}

savm_error_e savm_cpu_regwrite(savm_t* vm,uint64_t i,uint64_t val) {
	switch(i) {
		case 0: /* flags */
			if(val & SAVM_CPU_REG_FLAG_INTR && !(vm->cpu.regs.flags & SAVM_CPU_REG_FLAG_INTR)) val &= ~SAVM_CPU_REG_FLAG_INTR;
			if(val & SAVM_CPU_REG_FLAG_PAGING && !(vm->cpu.regs.flags & SAVM_CPU_REG_FLAG_PAGING)) val &= ~SAVM_CPU_REG_FLAG_PAGING;
			if(val & SAVM_CPU_REG_FLAG_PRIV_KERN && !(vm->cpu.regs.flags & SAVM_CPU_REG_FLAG_PRIV_KERN)) val &= ~SAVM_CPU_REG_FLAG_PRIV_KERN;
			if(val & SAVM_CPU_REG_FLAG_PRIV_USER && !(vm->cpu.regs.flags & SAVM_CPU_REG_FLAG_PRIV_USER)) val &= ~SAVM_CPU_REG_FLAG_PRIV_USER;
			
			if(!(vm->cpu.regs.flags & SAVM_CPU_REG_FLAG_PAGING) && val & SAVM_CPU_REG_FLAG_PAGING) {
				if(vm->io.pgdir == NULL) {
					savm_error_e err = savm_cpu_intr(vm,SAVM_CPU_INT_FAULT);
					if(err != SAVM_ERROR_NONE) return err;
				}
			}
			vm->cpu.regs.flags = val;
			break;
		case 1: /* tmp */
			vm->cpu.regs.tmp = val;
			break;
		case 2: /* sp */
		case 3: /* ip */
		case 4: /* pc */
		case 5: /* cycle */
			return SAVM_ERROR_INVAL_ADDR;
		case 6 ... 16: /* data */
			return vm->cpu.regs.data[i-6];
		case 17 ... 27: /* index */
			return vm->cpu.regs.index[i-17];
		case 28 ... 38: /* addr */
			return vm->cpu.regs.addr[i-28];
		case 39 ... 49: /* ptr */
			return vm->cpu.regs.ptr[i-39];
		default: return SAVM_ERROR_INVAL_ADDR;
	}
	return SAVM_ERROR_NONE;
}

savm_error_e savm_cpu_cycle(savm_t* vm) {
	/* Temporary patch for issue #1 */
	if(vm->cpu.regs.pc == 0) vm->cpu.regs.pc = SAVM_IO_RAM_BASE+(vm->cpu.regs.cycle*3);
	
	/* Fetches the instruction from memory */
	savm_error_e err = savm_ioctl_read(vm,vm->cpu.regs.pc,&vm->cpu.regs.ip);
	if(err != SAVM_ERROR_NONE) return err;
	
	/* Decodes the instruction */
	uint64_t addr;
	uint64_t val;
	
	err = savm_ioctl_read(vm,vm->cpu.regs.pc+1,&addr);
	if(err != SAVM_ERROR_NONE) return err;
	
	err = savm_ioctl_read(vm,vm->cpu.regs.pc+2,&val);
	if(err != SAVM_ERROR_NONE) return err;
	
	vm->cpu.regs.pc += 3;
	
	/* Execute the instruction */
	switch(vm->cpu.regs.ip) {
		case 0: /* NOP */
			vm->cpu.running = 0;
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
				
				vm->cpu.regs.tmp = a == b;
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
				
				vm->cpu.regs.tmp = a == b;
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
				
				if(vm->cpu.regs.tmp) vm->cpu.regs.pc = a;
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
				
				if(vm->cpu.regs.tmp) vm->cpu.regs.pc = a;
			}
			break;
		case 27: /* JIT */
			if(vm->cpu.regs.tmp) vm->cpu.regs.pc = addr;
			break;
		case 28: /* JMPR */
			{
				uint64_t a;
				err = savm_cpu_regread(vm,addr,&vm->cpu.regs.pc);
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
				err = savm_ioctl_read(vm,addr,&vm->cpu.regs.pc);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
				if(err == SAVM_ERROR_INVAL_ADDR || err != SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
			}
			break;
		case 30: /* JMP */
			vm->cpu.regs.pc = addr;
			break;
		case 31: /* CALLR */
			if(vm->cpu.regs.sp < SAVM_CPU_STACK_SIZE) {
				vm->cpu.stack[vm->cpu.regs.sp++] = vm->cpu.regs.pc;
				err = savm_cpu_regread(vm,addr,&vm->cpu.regs.pc);
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
			if(vm->cpu.regs.sp < SAVM_CPU_STACK_SIZE) {
				vm->cpu.stack[vm->cpu.regs.sp++] = vm->cpu.regs.pc;
				err = savm_ioctl_read(vm,addr,&vm->cpu.regs.pc);
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
			if(vm->cpu.regs.sp < SAVM_CPU_STACK_SIZE) {
				vm->cpu.stack[vm->cpu.regs.sp++] = vm->cpu.regs.pc;
				vm->cpu.regs.pc = addr;
			} else {
				err = savm_cpu_intr(vm,SAVM_CPU_INT_STACK_OVERFLOW);
				if(err != SAVM_ERROR_NONE) return err;
			}
			break;
		case 34: /* RET */
			if(vm->cpu.regs.sp < SAVM_CPU_STACK_SIZE) vm->cpu.regs.pc = vm->cpu.stack[vm->cpu.regs.sp--];
			else {
				err = savm_cpu_intr(vm,SAVM_CPU_INT_STACK_OVERFLOW);
				if(err != SAVM_ERROR_NONE) return err;
			}
			break;
		case 35: /* PUSHR */
			if(vm->cpu.regs.sp < SAVM_CPU_STACK_SIZE) {
				err = savm_cpu_regread(vm,addr,&vm->cpu.stack[vm->cpu.regs.sp++]);
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
			if(vm->cpu.regs.sp < SAVM_CPU_STACK_SIZE) {
				err = savm_ioctl_read(vm,addr,&vm->cpu.stack[vm->cpu.regs.sp++]);
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
			if(vm->cpu.regs.sp < SAVM_CPU_STACK_SIZE) {
				err = savm_cpu_regwrite(vm,addr,vm->cpu.stack[vm->cpu.regs.sp--]);
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
			if(vm->cpu.regs.sp < SAVM_CPU_STACK_SIZE) {
				err = savm_ioctl_write(vm,addr,vm->cpu.stack[vm->cpu.regs.sp--]);
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
			if(vm->cpu.regs.flags & SAVM_CPU_REG_FLAG_PRIV_KERN) {
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
			if(vm->cpu.regs.flags & SAVM_CPU_REG_FLAG_PRIV_KERN) {
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
			if(vm->cpu.regs.flags & SAVM_CPU_REG_FLAG_PRIV_KERN) {
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
			if(vm->cpu.regs.flags & SAVM_CPU_REG_FLAG_INTR || vm->cpu.regs.flags & SAVM_CPU_REG_FLAG_PRIV_KERN) {
				memcpy(&vm->cpu.regs,&vm->cpu.iregs,sizeof(savm_cpu_regs_t));
			} else {
				err = savm_cpu_intr(vm,SAVM_CPU_INT_FAULT);
				if(err != SAVM_ERROR_NONE) return err;
			}
		case 49: /* LDITBLR */
			if(vm->cpu.regs.flags & SAVM_CPU_REG_FLAG_PRIV_KERN) {
				err = savm_cpu_regread(vm,addr,&addr);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR) return err;
				if(err == SAVM_ERROR_INVAL_ADDR) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				for(uint64_t i = 0;i < SAVM_CPU_IVT_SIZE;i++) {
					err = savm_ioctl_read(vm,addr+i,&vm->cpu.ivt[i]);
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
			if(vm->cpu.regs.flags & SAVM_CPU_REG_FLAG_PRIV_KERN) {
				err = savm_ioctl_read(vm,addr,&addr);
				if(err != SAVM_ERROR_NONE && err != SAVM_ERROR_INVAL_ADDR && err != SAVM_ERROR_NOTMAPPED) return err;
					if(err == SAVM_ERROR_INVAL_ADDR || err == SAVM_ERROR_NOTMAPPED) {
					err = savm_cpu_intr(vm,SAVM_CPU_INT_BADADDR);
					if(err != SAVM_ERROR_NONE) return err;
					break;
				}
				
				for(uint64_t i = 0;i < SAVM_CPU_IVT_SIZE;i++) {
					err = savm_ioctl_read(vm,addr+i,&vm->cpu.ivt[i]);
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
			if(vm->cpu.regs.flags & SAVM_CPU_REG_FLAG_PRIV_KERN) {
				vm->cpu.running = 0;
			} else {
				err = savm_cpu_intr(vm,SAVM_CPU_INT_BADPERM);
				if(err != SAVM_ERROR_NONE) return err;
			}
			break;
		case 52: /* RST */
			if(vm->cpu.regs.flags & SAVM_CPU_REG_FLAG_PRIV_KERN) {
				err = savm_reset(vm);
				if(err != SAVM_ERROR_NONE) return err;
				vm->cpu.running = 1;
				vm->cpu.regs.cycle = -1;
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
	
	if(vm->cpu.regs.flags & SAVM_CPU_REG_FLAG_INTR && vm->cpu.regs.flags & SAVM_CPU_REG_FLAG_ENIRQ && vm->cpu.irqSize > 0) {
		uint64_t irq = vm->cpu.irqs[0];
		vm->cpu.irqs[0] = -1;
		err = savm_cpu_intr(vm,irq);
		if(err != SAVM_ERROR_NONE) return err;
	}
	
	vm->cpu.regs.cycle++;
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
	if(vm->cpu.regs.flags & SAVM_CPU_REG_FLAG_PAGING) {
		for(uint64_t t = 0;t < vm->io.pgdir->tableCount;t++) {
			for(uint64_t p = 0;p < vm->io.pgdir->tables[t].size;p++) {
				if(vm->io.pgdir->tables[t].page[p].address <= addr && vm->io.pgdir->tables[t].page[p].address+vm->io.pgdir->tables[t].page[p].size > addr) {
					if(vm->cpu.regs.flags & SAVM_CPU_REG_FLAG_PRIV_KERN && !(vm->io.pgdir->tables[t].page[p].perms & SAVM_CPU_PAGING_PERM_KERN)) {
						savm_error_e err = savm_cpu_intr(vm,SAVM_CPU_INT_BADPERM);
						if(err != SAVM_ERROR_NONE) return err;
					}
					if(vm->cpu.regs.flags & SAVM_CPU_REG_FLAG_PRIV_USER && !(vm->io.pgdir->tables[t].page[p].perms & SAVM_CPU_PAGING_PERM_USER)) {
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
	if(vm->cpu.regs.flags & SAVM_CPU_REG_FLAG_PAGING) {
		for(uint64_t t = 0;t < vm->io.pgdir->tableCount;t++) {
			for(uint64_t p = 0;p < vm->io.pgdir->tables[t].size;p++) {
				if(vm->io.pgdir->tables[t].page[p].address <= addr && vm->io.pgdir->tables[t].page[p].address+vm->io.pgdir->tables[t].page[p].size > addr) {
					if(vm->cpu.regs.flags & SAVM_CPU_REG_FLAG_PRIV_KERN && !(vm->io.pgdir->tables[t].page[p].perms & SAVM_CPU_PAGING_PERM_KERN)) {
						savm_error_e err = savm_cpu_intr(vm,SAVM_CPU_INT_BADPERM);
						if(err != SAVM_ERROR_NONE) return err;
					}
					if(vm->cpu.regs.flags & SAVM_CPU_REG_FLAG_PRIV_USER && !(vm->io.pgdir->tables[t].page[p].perms & SAVM_CPU_PAGING_PERM_USER)) {
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
	}
	return 0;
}

void savm_ioctl_ioctl_write(savm_t* vm,uint64_t i,uint64_t data) {
	switch(i) {
		case 1:
			if(vm->cpu.regs.flags & SAVM_CPU_REG_FLAG_PRIV_USER) {
				savm_error_e err = savm_cpu_intr(vm,SAVM_CPU_INT_BADPERM);
				if(err != SAVM_ERROR_NONE) {
					vm->cpu.running = 0;
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
	}
}

uint64_t savm_ioctl_ram_read(savm_t* vm,uint64_t i) {
	return vm->io.ram[i];
}

void savm_ioctl_ram_write(savm_t* vm,uint64_t i,uint64_t data) {
	vm->io.ram[i] = data;
}
