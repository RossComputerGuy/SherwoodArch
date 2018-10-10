#include <sasmc/opcode.h>
#include <argp.h>
#include <stdbool.h>
#include <stdlib.h>

#define MAX_FILES_IN 10

const char* argp_program_version = "sasmc 1.0";
const char* argp_program_bug_address = "<spaceboyross@yandex.com>";

static char doc[] = "SASMC - The Sherwood Architecture Assembler";
static char args_doc[] = "SOURCE";

static struct argp_option options[] = {
	{"verbose",'v',0,0,"Produce verbose output"},
	{"output",'o',"FILE",0,"Output to FILE instead of a.out"},
	{0}
};

struct arguments {
	char* file;
	char* output;
	bool verbose;
};

static error_t parse_opt(int key,char* arg,struct argp_state* state) {
	struct arguments* arguments = state->input;
	switch(key) {
		case 'v':
			arguments->verbose = true;
			break;
		case 'o':
			arguments->output = arg;
			break;
		case ARGP_KEY_ARG:
			if(state->arg_num >= MAX_FILES_IN) argp_usage(state);
			arguments->file = arg;
			break;
		case ARGP_KEY_END:
			if(state->arg_num < 1) argp_usage(state);
		default: return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp argp = { options, parse_opt, args_doc, doc };

int main(int argc,char** argv) {
	struct arguments arguments;
	arguments.verbose = false;
	arguments.output = "a.out";
	argp_parse(&argp,argc,argv,0,0,&arguments);
	
	uint64_t* instrs = compilefile(arguments.file,arguments.verbose);
	if(instrs == NULL) {
		fprintf(stderr,"Failed to compile \"%s\"\n",arguments.file);
		return EXIT_FAILURE;
	}
	FILE* tmp = fopen(arguments.file,"r");
	if(tmp == NULL) {
		fprintf(stderr,"Failed to compile \"%s\"\n",arguments.file);
		return EXIT_FAILURE;
	}
	int c = getc(tmp);
	int size = 0;
	while(c != EOF) {
		if(c == '\n') size++;
		c = getc(tmp);
	}
	fclose(tmp);
	FILE* fp = fopen(arguments.output,"wb");
	if(fp == NULL) {
		fprintf(stderr,"Failed to compile \"%s\"\n",arguments.file);
		return EXIT_FAILURE;
	}
	fwrite(instrs,size*sizeof(uint64_t),1,fp);
	fclose(fp);
	return EXIT_SUCCESS;
} 
