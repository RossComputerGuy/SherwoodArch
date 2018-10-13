const chevrotain = require("chevrotain");
const createToken = chevrotain.createToken;
const Lexer = chevrotain.Lexer;

const TOKEN_INSTRUCTIONS = {
	"nop": createToken({ name: "nop", pattern: /[nop,NOP]+/ }),
	"add": createToken({ name: "add", pattern: /[add,ADD]+/ }),
	"sub": createToken({ name: "sub", pattern: /[sub,SUB]+/ }),
	"mul": createToken({ name: "mul", pattern: /[mul,MUL]+/ }),
	"div": createToken({ name: "div", pattern: /[div,DIV]+/ }),
	"and": createToken({ name: "and", pattern: /[and,AND]+/ }),
	"or": createToken({ name: "or", pattern: /[or,OR]+/ }),
	"xor": createToken({ name: "xor", pattern: /[xor,XOR]+/ }),
	"nor": createToken({ name: "nor", pattern: /[nor,NOR]+/ }),
	"nand": createToken({ name: "nand", pattern: /[nand,NAND]+/ }),
	"lshift": createToken({ name: "lshift", pattern: /[lshift,LSHIFT]+/ }),
	"rshift": createToken({ name: "rshift", pattern: /[rshift,RSHIFT]+/ }),
	"cmp": createToken({ name: "cmp", pattern: /[cmp,CMP]+/ }),
	"jit": createToken({ name: "jit", pattern: /[jit,JIT]+/ }),
	"jmp": createToken({ name: "jmp", pattern: /[jmp,JMP]+/ }),
	"call": createToken({ name: "call", pattern: /[call,CALL]+/ }),
	"ret": createToken({ name: "ret", pattern: /[ret,RET]+/ }),
	"push": createToken({ name: "push", pattern: /[push,PUSH]+/ }),
	"pop": createToken({ name: "pop", pattern: /[pop,POP]+/ }),
	"mov": createToken({ name: "mov", pattern: /[mov,MOV]+/ }),
	"sto": createToken({ name: "sto", pattern: /[sto,STO]+/ }),
	"int": createToken({ name: "int", pattern: /[int,INT]+/ }),
	"iret": createToken({ name: "iret", pattern: /[iret,IRET]+/ }),
	"lditbl": createToken({ name: "lditbl", pattern: /[lditbl,LDITBL]+/ }),
	"hlt": createToken({ name: "hlt", pattern: /[hlt,HLT]+/ }),
	"rst": createToken({ name: "rst", pattern: /[rst,RST]+/ })
};

const TOKEN_BASE = {
	"address": createToken({ name: "address", pattern: /\$0x[0-9A-F]+/ }),
	"char": createToken({ name: "char", pattern: /'[.*]'/ }),
	"comma": createToken({ name: "comma", pattern: /,/ }),
	"comment": createToken({ name: "comment", pattern: /#.*/, group: chevrotain.Lexer.SKIPPED }),
	"fn": createToken({ name: "fn", pattern: /[a-zA-Z0-9]+:/ }),
	"integer": createToken({ name: "integer", pattern: /0|[1-9]\d+/ }),
	"register": createToken({ name: "register", pattern: /%[a-zA-Z0-9]+/ }),
	"whitespace": createToken({ name: "whitespace", pattern: /\s+/, group: chevrotain.Lexer.SKIPPED })
};

const all_tokens = (() => {
	var tokens = [];
	for(var token in TOKEN_BASE) tokens.push(TOKEN_BASE[token]);
	for(var token in TOKEN_INSTRUCTIONS) tokens.push(TOKEN_INSTRUCTIONS[token]);
	return tokens;
})();

module.exports = new Lexer(all_tokens);
module.exports.all_tokens = all_tokens;
module.exports.TOKEN_BASE = TOKEN_BASE;
module.exports.TOKEN_INSTRUCTIONS = TOKEN_INSTRUCTIONS;
