#ifndef __SAVM_HARDWARE_UART_H_
#define __SAVM_HARDWARE_UART_H_ 1

#include <savm/error.h>
#include <stdio.h>

struct savm;

typedef struct {
	FILE* out;
	FILE* in;
	char input;
} savm_uart_t;

savm_error_e savm_uart_create(savm_uart_t* uart);
savm_error_e savm_uart_destroy(savm_uart_t* uart);
savm_error_e savm_uart_reset(savm_uart_t* uart,struct savm* vm);
savm_error_e savm_uart_cycle(savm_uart_t* uart,struct savm* vm);

#endif
