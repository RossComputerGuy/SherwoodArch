#ifndef __SAVM_UTILS_H_
#define __SAVM_UTILS_H_ 1

#define SAVM_BITS_GETMASK(index,size) (((1 << (size))-1) << (index))
#define SAVM_BITS_EXTRACT(value,index,size) (((value) & SAVM_BITS_GETMASK((index),(size))) >> (index))

#endif
