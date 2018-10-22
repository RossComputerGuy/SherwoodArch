const chevrotain = require("chevrotain");
const createToken = chevrotain.createToken;
const Lexer = chevrotain.Lexer;

const TOKEN_INSTRUCTIONS = {
	"nop": createToken({ name: "nop", pattern: /nop/ }),
	"add": createToken({ name: "add", pattern: /add/ }),
	"sub": createToken({ name: "sub", pattern: /sub/ }),
	"mul": createToken({ name: "mul", pattern: /mul/ }),
	"div": createToken({ name: "div", pattern: /div/ }),
	"and": createToken({ name: "and", pattern: /and/ }),
	"or": createToken({ name: "or", pattern: /or/ }),
	"xor": createToken({ name: "xor", pattern: /xor/ }),
	"nor": createToken({ name: "nor", pattern: /nor/ }),
	"nand": createToken({ name: "nand", pattern: /nand/ }),
	"mod": createToken({ name: "mod", pattern: /mod/ }),
	"lshift": createToken({ name: "lshift", pattern: /lshift/ }),
	"rshift": createToken({ name: "rshift", pattern: /rshift/ }),
	"cmp": createToken({ name: "cmp", pattern: /cmp/ }),
	"grtn": createToken({ name: "grtn", pattern: /grtn/ }),
	"lstn": createToken({ name: "grtn", pattern: /lstn/ }),
	"jit": createToken({ name: "jit", pattern: /jit/ }),
	"jmp": createToken({ name: "jmp", pattern: /jmp/ }),
	"call": createToken({ name: "call", pattern: /call/ }),
	"ret": createToken({ name: "ret", pattern: /ret/ }),
	"push": createToken({ name: "push", pattern: /push/ }),
	"pop": createToken({ name: "pop", pattern: /pop/ }),
	"mov": createToken({ name: "mov", pattern: /mov/ }),
	"int": createToken({ name: "int", pattern: /int/ }),
	"iret": createToken({ name: "iret", pattern: /iret/ }),
	"lditbl": createToken({ name: "lditbl", pattern: /lditbl/ }),
	"rst": createToken({ name: "rst", pattern: /rst/ })
};

const TOKEN_BASE = {
	"address": createToken({ name: "address", pattern: /\$0x[0-9A-F]+/ }),
	"char": createToken({ name: "char", pattern: /'(\\?[^'\n]|\\')'?/ }),
	"comma": createToken({ name: "comma", pattern: /,/ }),
	"comment": createToken({ name: "comment", pattern: /#.*/, group: chevrotain.Lexer.SKIPPED }),
	"fn": createToken({ name: "fn", pattern: /[a-zA-Z0-9]+:/ }),
	"identifier": createToken({ name: "identifier", pattern: /\w+/ }),
	"integer": createToken({ name: "integer", pattern: /0|[1-9]\d+/ }),
	"register": createToken({ name: "register", pattern: /%[a-zA-Z0-9]+/ }),
	"whitespace": createToken({ name: "whitespace", pattern: /\s+/, group: chevrotain.Lexer.SKIPPED })
};

const all_tokens = (() => {
	var tokens = [];
	for(var token in TOKEN_INSTRUCTIONS) tokens.push(TOKEN_INSTRUCTIONS[token]);
	for(var token in TOKEN_BASE) tokens.push(TOKEN_BASE[token]);
	return tokens;
})();

module.exports = new Lexer(all_tokens);
module.exports.all_tokens = all_tokens;
module.exports.TOKEN_BASE = TOKEN_BASE;
module.exports.TOKEN_INSTRUCTIONS = TOKEN_INSTRUCTIONS;
