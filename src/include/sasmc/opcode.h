#ifndef __SASMC_OPCODE_H_
#define __SASMC_OPCODE_H_ 1

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

bool string_validopcode(char* str);
uint64_t string2int(char* str);
uint64_t string2opcode(char* str);
uint64_t* compileinstr(char* str,bool verbose);
size_t getcompiledsize(char* str);
uint64_t* compilestr(char* str,bool verbose);
uint64_t* compilefp(FILE* fp,size_t sz,bool verbose);
uint64_t* compilefile(char* path,bool verbose);

#endif
