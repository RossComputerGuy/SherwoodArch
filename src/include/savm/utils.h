#ifndef __SAVM_UTILS_H_
#define __SAVM_UTILS_H_ 1

#define SAVM_BITS_EXTRACT(value,begin,size) ((value >> begin) & (1 << ((begin+size)-begin))-1)

#endif
