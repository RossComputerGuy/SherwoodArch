#ifndef __SAVM_VM_H_
#define __SAVM_VM_H_ 1

#include <savm/hardware/mailbox.h>
#include <savm/hardware/rtc.h>
#include <savm/hardware/uart.h>
#include <savm/error.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* CPU -> Flags */

#ifndef SAVM_CPU_REG_FLAG_INTR
#define SAVM_CPU_REG_FLAG_INTR (1 << 0)
#endif

#ifndef SAVM_CPU_REG_FLAG_ENIRQ
#define SAVM_CPU_REG_FLAG_ENIRQ (1 << 1)
#endif

/* CPU -> sizes */

#ifndef SAVM_CPU_STACK_SIZE
#define SAVM_CPU_STACK_SIZE 20
#endif

#ifndef SAVM_CPU_IVT_SIZE
#define SAVM_CPU_IVT_SIZE 7
#endif

/* CPU -> IVT */

#ifndef SAVM_CPU_INT_STACK_OVERFLOW
#define SAVM_CPU_INT_STACK_OVERFLOW 0
#endif

#ifndef SAVM_CPU_INT_FAULT
#define SAVM_CPU_INT_FAULT 1
#endif

#ifndef SAVM_CPU_INT_BADADDR
#define SAVM_CPU_INT_BADADDR 2
#endif

#ifndef SAVM_CPU_INT_DIVBYZERO
#define SAVM_CPU_INT_DIVBYZERO 3
#endif

#ifndef SAVM_CPU_INT_BADINSTR
#define SAVM_CPU_INT_BADINSTR 4
#endif

#ifndef SAVM_CPU_INT_TIMER
#define SAVM_CPU_INT_TIMER 5
#endif

#ifndef SAVM_CPU_INT_MAILBOX
#define SAVM_CPU_INT_MAILBOX 6
#endif

/* IO CTRL -> Mailbox */

#ifndef SAVM_IO_MAILBOX_BASE
#define SAVM_IO_MAILBOX_BASE 0x10000000
#endif

#ifndef SAVM_IO_MAILBOX_SIZE
#define SAVM_IO_MAILBOX_SIZE 0x00000009
#endif

#ifndef SAVM_IO_MAILBOX_END
#define SAVM_IO_MAILBOX_END (SAVM_IO_MAILBOX_BASE+SAVM_IO_MAILBOX_SIZE)
#endif

/* IO CTRL -> RTC */

#ifndef SAVM_IO_RTC_BASE
#define SAVM_IO_RTC_BASE (SAVM_IO_MAILBOX_END+1)
#endif

#ifndef SAVM_IO_RTC_SIZE
#define SAVM_IO_RTC_SIZE 0x00000007
#endif

#ifndef SAVM_IO_RTC_END
#define SAVM_IO_RTC_END (SAVM_IO_RTC_BASE+SAVM_IO_RTC_SIZE)
#endif

/* IO CTRL -> UART */

#ifndef SAVM_IO_UART_BASE
#define SAVM_IO_UART_BASE (SAVM_IO_RTC_END+1)
#endif

#ifndef SAVM_IO_UART_SIZE
#define SAVM_IO_UART_SIZE 0x00000003
#endif

#ifndef SAVM_IO_UART_END
#define SAVM_IO_UART_END (SAVM_IO_UART_BASE+SAVM_IO_UART_SIZE)
#endif

/* IO CTRL -> RAM */

#ifndef SAVM_IO_RAM_BASE
#define SAVM_IO_RAM_BASE 0xA0000000
#endif

#ifndef SAVM_IO_RAM_SIZE
#define SAVM_IO_RAM_SIZE 0x40000000
#endif

#ifndef SAVM_IO_RAM_END
#define SAVM_IO_RAM_END (SAVM_IO_RAM_BASE+SAVM_IO_RAM_SIZE)
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

typedef struct {
	uint64_t flags; /* Flags */
	uint64_t tmp; /* Temporary Data */
	uint64_t sp; /* Stack Pointer */
	uint64_t ip; /* Instruction Pointer */
	uint64_t pc; /* Program Counter */
	uint64_t cycle; /* Cycle counter */
	uint64_t data[10]; /* Data */
	uint64_t index[10]; /* Indexes */
	uint64_t addr[10]; /* Addresses */
	uint64_t ptr[10]; /* Pointers */
} savm_cpu_regs_t;

typedef struct savm {
	struct {
		savm_cpu_regs_t regs;
		
		/* The registers before the interrupt */
		savm_cpu_regs_t iregs;
		
		uint64_t stack[SAVM_CPU_STACK_SIZE];
        uint64_t ivt[SAVM_CPU_IVT_SIZE];
        uint64_t intr;
		int running;
		
		uint64_t* irqs;
		size_t irqSize;
	} cpu;
	struct {
		uint64_t* ram;
		
		size_t mmapSize;
		savm_io_mmap_entry_t* mmap;
	} io;
	
	savm_mailbox_t mailbox;
	savm_rtc_t rtc;
	savm_uart_t uart;
} savm_t;

savm_error_e savm_create(savm_t* vm);
savm_error_e savm_destroy(savm_t* vm);
savm_error_e savm_reset(savm_t* vm);

savm_error_e savm_cpu_intr(savm_t* vm,uint64_t intr);
savm_error_e savm_cpu_regread(savm_t* vm,uint64_t i,uint64_t* val);
savm_error_e savm_cpu_regwrite(savm_t* vm,uint64_t i,uint64_t val);
savm_error_e savm_cpu_cycle(savm_t* vm);

savm_error_e savm_ioctl_loadfile(savm_t* vm,uint64_t addr,char* path);
savm_error_e savm_ioctl_loadfp(savm_t* vm,uint64_t addr,uint64_t size,FILE* fp);
savm_error_e savm_ioctl_dumpfile(savm_t* vm,uint64_t addr,uint64_t size,char* path);
savm_error_e savm_ioctl_dumpfp(savm_t* vm,uint64_t addr,uint64_t size,FILE* fp);
savm_error_e savm_ioctl_mmap(savm_t* vm,uint64_t addr,uint64_t end,savm_ioctl_read_p read,savm_ioctl_write_p write);
savm_error_e savm_ioctl_read(savm_t* vm,uint64_t addr,uint64_t* data);
savm_error_e savm_ioctl_write(savm_t* vm,uint64_t addr,uint64_t data);

#endif
