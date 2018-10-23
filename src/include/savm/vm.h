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

/* Instruction -> Address Modes */

#ifndef SAVM_INSTR_ADDRMODE_DEFAULT
#define SAVM_INSTR_ADDRMODE_DEFAULT SAVM_INSTR_ADDRMODE_REG
#endif

#ifndef SAVM_INSTR_ADDRMODE_REG
#define SAVM_INSTR_ADDRMODE_REG 0
#endif

#ifndef SAVM_INSTR_ADDRMODE_ADDR
#define SAVM_INSTR_ADDRMODE_ADDR 1
#endif

#ifndef SAVM_INSTR_ADDRMODE_RAW
#define SAVM_INSTR_ADDRMODE_RAW 2
#endif

/* Instruction -> Flags */

#ifndef SAVM_INSTR_FLAG_SIGNED
#define SAVM_INSTR_FLAG_SIGNED (1 << 0)
#endif

#ifndef SAVM_INSTR_FLAG_UNSIGNED
#define SAVM_INSTR_FLAG_UNSIGNED ~SAVM_INSTR_FLAG_SIGNED
#endif

/* CPU -> Flags */

#ifndef SAVM_CPU_REG_FLAG_INTR
#define SAVM_CPU_REG_FLAG_INTR (1 << 0)
#endif

#ifndef SAVM_CPU_REG_FLAG_ENIRQ
#define SAVM_CPU_REG_FLAG_ENIRQ (1 << 1)
#endif

#ifndef SAVM_CPU_REG_FLAG_PAGING
#define SAVM_CPU_REG_FLAG_PAGING (1 << 2)
#endif

#ifndef SAVM_CPU_REG_FLAG_PRIV_KERN
#define SAVM_CPU_REG_FLAG_PRIV_KERN (1 << 3)
#endif

#ifndef SAVM_CPU_REG_FLAG_PRIV_USER
#define SAVM_CPU_REG_FLAG_PRIV_USER (1 << 4)
#endif

/* CPU -> sizes */

#ifndef SAVM_CPU_STACK_SIZE
#define SAVM_CPU_STACK_SIZE 20
#endif

#ifndef SAVM_CPU_IVT_SIZE
#define SAVM_CPU_IVT_SIZE 9
#endif

#ifndef SAVM_CPU_CORE_COUNT
#define SAVM_CPU_CORE_COUNT 4
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

#ifndef SAVM_CPU_INT_BADPERM
#define SAVM_CPU_INT_BADPERM 5
#endif

#ifndef SAVM_CPU_INT_TIMER
#define SAVM_CPU_INT_TIMER 6
#endif

#ifndef SAVM_CPU_INT_MAILBOX
#define SAVM_CPU_INT_MAILBOX 7
#endif

#ifndef SAVM_CPU_INT_SYSCALL
#define SAVM_CPU_INT_SYSCALL 8
#endif

/* CPU -> Paging */

#ifndef SAVM_CPU_PAGING_FLAG_PRESENT
#define SAVM_CPU_PAGING_FLAG_PRESENT (1 << 0)
#endif

#ifndef SAVM_CPU_PAGING_FLAG_ACCESSED
#define SAVM_CPU_PAGING_FLAG_ACCESSED (1 << 1)
#endif

#ifndef SAVM_CPU_PAGING_FLAG_DIRTY
#define SAVM_CPU_PAGING_FLAG_DIRTY (1 << 2)
#endif

#ifndef SAVM_CPU_PAGING_PERM_KERN
#define SAVM_CPU_PAGING_PERM_KERN (1 << 0)
#endif

#ifndef SAVM_CPU_PAGING_PERM_USER
#define SAVM_CPU_PAGING_PERM_USER (1 << 1)
#endif

#ifndef SAVM_CPU_PAGING_PERM_ALL
#define SAVM_CPU_PAGING_PERM_ALL (SAVM_CPU_PAGING_PERM_KERN | SAVM_CPU_PAGING_PERM_USER)
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

/* IO CTRL */

#ifndef SAVM_IO_IOCTL_BASE
#define SAVM_IO_IOCTL_BASE (SAVM_IO_UART_END+1)
#endif

#ifndef SAVM_IO_IOCTL_SIZE
#define SAVM_IO_IOCTL_SIZE 0x00000005
#endif

#ifndef SAVM_IO_IOCTL_END
#define SAVM_IO_IOCTL_END (SAVM_IO_IOCTL_BASE+SAVM_IO_IOCTL_SIZE)
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

typedef struct {
	uint32_t flags; /* Page flags */
	uint32_t perms; /* Page permissions */
	uint64_t size; /* Page size */
	uint64_t paddress; /* Page physical address */
	uint64_t vaddress; /* Page virtual address */
} savm_page_t;

typedef struct {
	uint64_t size;
	savm_page_t* page; /* Table entries */
} savm_pagetbl_t;

typedef struct {
	uint64_t tableCount;
	savm_pagetbl_t* tables;
} savm_pagedir_t;

typedef struct {
	savm_cpu_regs_t regs;
	
	/* The registers before the interrupt */
	savm_cpu_regs_t iregs;
		
	uint64_t stack[SAVM_CPU_STACK_SIZE];
	uint64_t ivt[SAVM_CPU_IVT_SIZE];
	uint64_t intr;
	int running;
	
	uint64_t* irqs;
	size_t irqSize;
} savm_cpu_core_t;

typedef struct savm {
	struct {
		savm_cpu_core_t cores[SAVM_CPU_CORE_COUNT];
		uint8_t currentCore;
	} cpu;
	struct {
		uint64_t* ram;
		
		size_t mmapSize;
		savm_io_mmap_entry_t* mmap;
		
		savm_pagedir_t* pgdir;
		
		uint8_t selectedCore;
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
savm_error_e savm_cpu_cycle_core(savm_t* vm,uint8_t i);
savm_error_e savm_cpu_cycle(savm_t* vm);

savm_error_e savm_ioctl_loadfile(savm_t* vm,uint64_t addr,char* path);
savm_error_e savm_ioctl_loadfp(savm_t* vm,uint64_t addr,uint64_t size,FILE* fp);
savm_error_e savm_ioctl_dumpfile(savm_t* vm,uint64_t addr,uint64_t size,char* path);
savm_error_e savm_ioctl_dumpfp(savm_t* vm,uint64_t addr,uint64_t size,FILE* fp);
savm_error_e savm_ioctl_mmap(savm_t* vm,uint64_t addr,uint64_t end,savm_ioctl_read_p read,savm_ioctl_write_p write);
savm_error_e savm_ioctl_read(savm_t* vm,uint64_t addr,uint64_t* data);
savm_error_e savm_ioctl_write(savm_t* vm,uint64_t addr,uint64_t data);

#endif
