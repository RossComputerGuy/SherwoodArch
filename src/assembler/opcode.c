#include <sasmc/opcode.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

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
	if(strstr(str," ") != NULL) {
		str = strtok(str," ");
		if(str == NULL) return false;
		while(str[0] == ' ') str++;
	}
	str = strlwr(str);
	if(str == NULL) return false;
	
	/* Check if the string matches the opcode name */
	if(!strcmp(str,"nop")) return 0;
	if(!strcmp(str,"addr")) return 1;
	if(!strcmp(str,"addm")) return 2;
	if(!strcmp(str,"subr")) return 3;
	if(!strcmp(str,"subm")) return 4;
	if(!strcmp(str,"mulr")) return 5;
	if(!strcmp(str,"mulm")) return 6;
	if(!strcmp(str,"divr")) return 7;
	if(!strcmp(str,"divm")) return 8;
	if(!strcmp(str,"andr")) return 9;
	if(!strcmp(str,"andm")) return 10;
	if(!strcmp(str,"orr")) return 11;
	if(!strcmp(str,"orm")) return 12;
	if(!strcmp(str,"xorr")) return 13;
	if(!strcmp(str,"xorm")) return 14;
	if(!strcmp(str,"norr")) return 15;
	if(!strcmp(str,"norm")) return 16;
	if(!strcmp(str,"nandr")) return 17;
	if(!strcmp(str,"nandm")) return 18;
	if(!strcmp(str,"lshiftr")) return 19;
	if(!strcmp(str,"lshiftm")) return 20;
	if(!strcmp(str,"rshiftr")) return 21;
	if(!strcmp(str,"rshiftm")) return 22;
	if(!strcmp(str,"cmpr")) return 23;
	if(!strcmp(str,"cmpm")) return 24;
	if(!strcmp(str,"jitr")) return 25;
	if(!strcmp(str,"jitm")) return 26;
	if(!strcmp(str,"jit")) return 27;
	if(!strcmp(str,"jmpr")) return 28;
	if(!strcmp(str,"jmpm")) return 29;
	if(!strcmp(str,"jmp")) return 30;
	if(!strcmp(str,"callr")) return 31;
	if(!strcmp(str,"callm")) return 32;
	if(!strcmp(str,"call")) return 33;
	if(!strcmp(str,"ret")) return 34;
	if(!strcmp(str,"pushr")) return 35;
	if(!strcmp(str,"pushm")) return 36;
	if(!strcmp(str,"popr")) return 37;
	if(!strcmp(str,"popm")) return 38;
	if(!strcmp(str,"movrr")) return 39;
	if(!strcmp(str,"movrm")) return 40;
	if(!strcmp(str,"movmr")) return 41;
	if(!strcmp(str,"movmm")) return 42;
	if(!strcmp(str,"stor")) return 43;
	if(!strcmp(str,"stom")) return 44;
	if(!strcmp(str,"intr")) return 45;
	if(!strcmp(str,"intm")) return 46;
	if(!strcmp(str,"int")) return 47;
	if(!strcmp(str,"iret")) return 48;
	if(!strcmp(str,"lditblr")) return 49;
	if(!strcmp(str,"lditblm")) return 50;
	if(!strcmp(str,"hlt")) return 51;
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
		char* addrStr = strtok(NULL,", ");
		if(addrStr == NULL) return NULL;
		
		char* valStr = strtok(NULL,", ");
		if(valStr == NULL) return NULL;
		
		addr = string2int(addrStr);
		val = string2int(valStr);
	}
	
	if(opcode == 27 || opcode == 30 || opcode == 33 || (opcode >= 47 && opcode < 51)) {
		addr = string2int(strtok(NULL," "));
	}
	
	if(verbose) printf("Compiled instruction: 0x%x 0x%x 0x%x\n",opcode,addr,val);
	
	return (uint64_t[3]){opcode,addr,val};
}

size_t getcompiledsize(char* str) {
	int lineCount = 0;
	for(size_t i = 0;i < strlen(str);i++) {
		if(str[i] == '\n') lineCount++;
	}
	return lineCount*3;
}

uint64_t* compilestr(char* str,bool verbose) {
	uint64_t* instrs = malloc(sizeof(uint64_t)*getcompiledsize(str));
	if(instrs == NULL) return NULL;
	
	size_t index = 0;
	char* pch = strtok(str,"\n");
	while(pch != NULL) {
		uint64_t* opcode = compileinstr(pch,verbose);
		if(opcode == NULL) {
			free(instrs);
			return NULL;
		}
		for(size_t i = 0;i < 3;i++) {
			printf("%d: 0x%x\n",index,opcode[i]);
			instrs[index++] = opcode[i];
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
