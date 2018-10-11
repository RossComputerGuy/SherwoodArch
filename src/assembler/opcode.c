#include <sasmc/opcode.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define INSTRUCTION_SET_SIZE 53

const char* instruction_set[INSTRUCTION_SET_SIZE] = {
	"nop","addr","addm","subr","subm","mulr","mulm","divr","divm",
	"andr","andm","orr","orm","xorr","xorm","norr","norm","nandr","nandm","lshiftr","lshiftm","rshiftr","rshiftm",
	"cmpr","cmpm","jitr","jitm","jit","jmpr","jmpm","jmp","callr","callm","call","ret",
	"pushr","pushm","popr","popm","movrr","movrm","movmr","movmm","stor","stom",
	"intr","intm","int","iret","lditblr","lditblm","hlt","rst"
};

/* Added since strwr is not included in libc */
char* strlwr(char* str) {
	for(size_t i = 0;i < strlen(str);i++) str[i] = tolower(str[i]);
	return str;
}

bool string_validopcode(char* str) {
	return string2opcode(str) != 0xFFFF;
}

uint64_t string2int(char* str) {
	if(str == NULL) return 0;
	if(str[0] == '0' && str[1] == 'x') return strtol(str+2,NULL,16);
	if(strlen(str) == 3 && str[0] == '\'' && str[2] == '\'') return str[1];
	return atoi(str);
}

uint64_t string2opcode(char* str) {
	char* str2 = malloc(strlen(str));
	if(str2 == NULL) return 0xFFFF;
	strcpy(str2,str);
	
	if(strstr(str2," ") != NULL) {
		str2 = strtok_r(str," ",&str2);
		if(str2 == NULL) goto error;
		while(str2[0] == ' ') str++;
	}
	str2 = strlwr(str2);
	if(str2 == NULL) goto error;
	
	/* Check if the string matches the opcode name */
	for(size_t i = 0;i < INSTRUCTION_SET_SIZE;i++) {
		if(!strcmp(str2,instruction_set[i])) {
			free(str2);
			return i;
		}
	}
error:
	free(str2);
	return 0xFFFF;
}

uint64_t* compileinstr(char* str,bool verbose) {
	uint64_t opcode = string2opcode(str);
	if(opcode == 0xFFFF) {
		fprintf(stderr,"Invalid instruction found: \"%s\"\n",str);
		return NULL;
	}
	uint64_t addr = 0;
	uint64_t val = 0;
	
	if((opcode >= 1 && opcode < 27) || opcode == 28 || opcode == 29 || opcode == 31 || opcode == 32 || (opcode >= 35 && opcode < 47)) {
		char* addrStr;
		strtok_r(str,", ",&addrStr);
		if(addrStr == NULL) return NULL;
		
		char* valStr;
		strtok_r(str,", ",&valStr);
		if(valStr == NULL) return NULL;
		
		addr = string2int(addrStr);
		val = string2int(valStr);
	}
	
	if(opcode == 27 || opcode == 30 || opcode == 33 || (opcode >= 47 && opcode < 51)) {
		addr = string2int(strtok(str," "));
	}
	
	if(verbose) printf("Compiled instruction: 0x%x 0x%x 0x%x\n",opcode,addr,val);
	
	return (uint64_t[3]){opcode,addr,val};
}

size_t getcompiledsize(char* str) {
	int lineCount = 0;
	char* pch = strtok(str,"\n");
	while(pch != NULL) {
		if(!!strcmp(pch,"\n") && pch[0] != '#') lineCount++;
		pch = strtok(NULL,"\n");
	}
	return lineCount*3;
}

uint64_t* compilestr(char* str,bool verbose) {
	uint64_t* instrs = malloc(sizeof(uint64_t)*getcompiledsize(str));
	if(instrs == NULL) return NULL;
	
	size_t index = 0;
	char* pch = strtok(str,"\n");
	while(pch != NULL) {
		if(!!strcmp(pch,"\n") && pch[0] != '#') {
			uint64_t* opcode = compileinstr(pch,verbose);
			if(opcode == NULL) {
				free(instrs);
				return NULL;
			}
			for(size_t i = 0;i < 3;i++) {
				printf("%d: 0x%x\n",index,opcode[i]);
				instrs[index++] = opcode[i];
			}
		}
		pch = strtok(NULL,"\n");
	}
	
	return instrs;
}

uint64_t* compilefp(FILE* fp,size_t sz,bool verbose) {
	char* str = malloc(sz);
	if(str == NULL) return NULL;
	fread(str,sz,1,fp);
	uint64_t* instrs = compilestr(str,verbose);
	free(str);
	return instrs;
}

uint64_t* compilefile(char* path,bool verbose) {
	FILE* fp = fopen(path,"r");
	if(fp == NULL) return NULL;
	struct stat st;
	stat(path,&st);
	uint64_t* instrs = compilefp(fp,st.st_size,verbose);
	fclose(fp);
	return instrs;
}
