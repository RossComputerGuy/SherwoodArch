#ifndef __SAVM_ERROR_H_
#define __SAVM_ERROR_H_ 1

typedef enum {
	SAVM_ERROR_NONE = 0,
	SAVM_ERROR_MEM,
	SAVM_ERROR_NOTMAPPED,
	SAVM_ERROR_MAPPED,
	SAVM_ERROR_INVAL_ADDR,
    SAVM_ERROR_INVAL_INT
} savm_error_e;

#endif
