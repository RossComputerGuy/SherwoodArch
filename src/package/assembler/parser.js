const jints = require("jints");
const Lexer = require("./lexer.js");
const vm = require("vm");
const _ = require("struct-fu");

const REGISTERS = (() => {
	var r = ["flags","tmp","sp","ip","pc","cycle"];
	var bigr = ["data","index","addr","ptr"];
	for(var br of bigr) {
		for(var i = 0;i < 10;i++) r.push(br+i);
	}
	return r;
})();

const INSTR = _.struct([
	_.struct("instr"[
		_.byte("unused",40),
		_.uint8("opcode"),
		_.uint8("addrmode"),
		_.uint8("flags")
	]),
	_.float64("address"),
	_.float64("data")
]);

const INSTR_ADDRMODE = {
	"DEFAULT": 0,
	"REG": 0,
	"ADDR": 1,
	"RAW": 2
};

const INSTR_SIZES = {
	"nop": 0,
	"add": 2,
	"sub": 2,
	"mul": 2,
	"div": 2,
	"and": 2,
	"or": 2,
	"xor": 2,
	"nor": 2,
	"nand": 2,
	"mod": 2,
	"lshift": 2,
	"rshift": 2,
	"cmp": 2,
	"grtn": 2,
	"lstn": 2,
	"jit": 1,
	"jmp": 1,
	"call": 1,
	"ret": 1,
	"push": 1,
	"pop": 1,
	"mov": 2,
	"int": 1,
	"iret": 1,
	"lditbl": 1,
	"rst": 0
};

class Token {
	constructor(type) {
		this.type = type;
		this.length = 0;
	}
	compile(parser) {}
}

class TokenInstruction extends Token {
	constructor(tokens) {
		super("instr");
		this.tokens = tokens;
		this.name = this.tokens[0].image;
		for(var i = 0;i < this.tokens.length;i++) {
			if(this.tokens[i] == null) throw new Error("Token cannot be null");
		}
		if(this.tokens.length == 1) {
			// This is here so the single token instructions don't cause the "Invalid token length" error to occur
		} else if(this.tokens.length == 2) {
			if(this.tokens[1].tokenType != Lexer.TOKEN_BASE.address
				&& this.tokens[1].tokenType != Lexer.TOKEN_BASE.register
				&& this.tokens[1].tokenType != Lexer.TOKEN_BASE.char
				&& this.tokens[1].tokenType != Lexer.TOKEN_BASE.identifier
				&& this.tokens[1].tokenType != Lexer.TOKEN_BASE.integer) throw new Error("Invalid type of token");
		} else if(this.tokens.length == 4) {
			if(this.tokens[1].tokenType != Lexer.TOKEN_BASE.address
				&& this.tokens[1].tokenType != Lexer.TOKEN_BASE.register) throw new Error("Invalid type of token");
			if(this.tokens[2].image != ",") throw new Error("Comma is needed");
			if(this.tokens[3].tokenType != Lexer.TOKEN_BASE.address
				&& this.tokens[3].tokenType != Lexer.TOKEN_BASE.register
				&& this.tokens[3].tokenType != Lexer.TOKEN_BASE.char
				&& this.tokens[3].tokenType != Lexer.TOKEN_BASE.identifier
				&& this.tokens[3].tokenType != Lexer.TOKEN_BASE.integer) throw new Error("Invalid type of token");
		} else throw new Error("Invalid token length");
		this.length = this.tokens.length-1;
	}
	compileAddress(token) {
		return parseInt(token.image.replace("$0x",""),16);
	}
	compileChar(token) {
		var sb = { result: token.image.slice(1,-1) };
		vm.runInNewContext("result = "+this.tokens[3].image+";",sb,"assembler.js");
		return Buffer.from(sb.result)[0];
	}
	compileRegister(token) {
		var str = token.image.replace("%","");
		if(REGISTERS.indexOf(str) == -1) throw new Error(str+" is not a register");
		return REGISTERS.indexOf(str);
	}
	compileFN(parser,token) {
		return 0xA0000000+parser.findFunctionOffset(token.image);
	}
	compileParam(parser,token) {
		if(token.tokenType == Lexer.TOKEN_BASE.register) return this.compileRegister(token);
		if(token.tokenType == Lexer.TOKEN_BASE.identifier) return this.compileFN(parser,token);
		if(token.tokenType == Lexer.TOKEN_BASE.address) return this.compileAddress(token);
		if(token.tokenType == Lexer.TOKEN_BASE.integer) {
			if(token.image.slice(0,2) == "0b") return parseInt(token.image.replace("0b",""),2);
			if(token.image.slice(0,2) == "0x") return parseInt(token.image.replace("0x",""),16);
			return parseInt(token.image);
		}
	}
	compile(parser) {
		var opcodes = {
			instr: {
				opcode: 0,
				addrmode: 0,
				flags: 0
			},
			address: 0,
			value: 0
		};
		
		if(this.tokens.length >= 2) {
			if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.register) opcodes.instr.addrmode = INSTR_ADDRMODE["REG"];
			if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.identifier
				|| this.tokens[1].tokenType == Lexer.TOKEN_BASE.address) opcodes.instr.addrmode = INSTR_ADDRMODE["ADDR"];
			if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.integer
				|| this.tokens[1].tokenType == Lexer.TOKEN_BASE.char) opcodes.instr.addrmode = INSTR_ADDRMODE["RAW"];
			
			opcodes.address = this.compileParam(this.tokens[1]);
		}
		
		if(this.tokens.length == 4) opcodes.value = this.compileParam(this.tokens[3]);
		
		switch(this.name) {
			case "nop":
				opcodes.instr.opcode = 0;
				break;
			case "add":
				opcodes.instr.opcode = 1;
				break;
			case "sub":
				opcodes.instr.opcode = 2;
				break;
			case "mul":
				opcodes.instr.opcode = 3;
				break;
			case "div":
				opcodes.instr.opcode = 4;
				break;
			case "and":
				opcodes.instr.opcode = 5;
				break;
			case "or":
				opcodes.instr.opcode = 6;
				break;
			case "xor":
				opcodes.instr.opcode = 7;
				break;
			case "nor":
				opcodes.instr.opcode = 8;
				break;
			case "nand":
				opcodes.instr.opcode = 9;
				break;
			case "mod":
				opcodes.instr.opcode = 10;
				break;
			case "lshift":
				opcodes.instr.opcode = 11;
				break;
			case "rshift":
				opcodes.instr.opcode = 12;
				break;
			case "cmp":
				opcodes.instr.opcode = 13;
				break;
			case "grtn":
				opcodes.instr.opcode = 14;
				break;
			case "lstn":
				opcodes.instr.opcode = 15;
				break;
			case "jit":
				opcodes.instr.opcode = 16;
				break;
			case "jmp":
				opcodes.instr.opcode = 17;
				break;
			case "call":
				opcodes.instr.opcode = 18;
				break;
			case "ret":
				opcodes.instr.opcode = 19;
				break;
			case "push":
				opcodes.instr.opcode = 20;
				break;
			case "pop":
				opcodes.instr.opcode = 21;
				break;
			case "mov":
				if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.register) opcodes.instr.opcode = 22;
				else if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.address || this.tokens[1].tokenType == Lexer.TOKEN_BASE.identifier) opcodes.instr.opcode = 23;
				else throw new Error("Address parameter is not a register or memory address");
				break;
			case "int":
				opcodes.instr.opcode = 24;
				break;
			case "iret":
				opcodes.instr.opcode = 25;
				break;
			case "lditbl":
				opcodes.instr.opcode = 26;
				break;
			case "rst":
				opcodes.instr.opcode = 27;
				break;
			default: throw new Error("Invalid instruction");
		}
		return INSTR.pack(opcodes);
	}
}

class TokenFunction extends Token {
	constructor(fnToken,tokens) {
		super("function");
		this.tokens = tokens;
		this.name = fnToken.image.replace(":","");
		this.length = 0;
		for(var t of this.tokens) {
			for(var a of t.tokens) {
				this.length++;
			}
		}
	}
	compile(parser) {
		var instrs = Buffer.alloc(this.length*(8*3));
		var index = 0;
		for(var token of this.tokens) {
			var instr = token.compile(parser);
			for(var i = 0;i < instr.length;i++) instrs[index++] = instr[i];
		}
		return instrs;
	}
}

class Parser {
	constructor() {
		this.errors = [];
		this.tokens = [];
		this.length = 0;
	}
	findFunctionOffset(name) {
		var offset = 0;
		for(var token of this.tokens) {
			if(token.type == "function" && token.name == name) return offset;
			else offset += token.length;
		}
		return null;
	}
	findFunction(name) {
		for(var token of this.tokens) {
			if(token.type == "function" && token.name == name) return token;
		}
		return null;
	}
	parseToken(token,lexerResult,index) {
		try {
			if(token.tokenType == Lexer.TOKEN_BASE.fn) {
				var end = lexerResult.tokens.length;
				for(var i = index+1;i < lexerResult.tokens.length;i++) {
					if(lexerResult.tokens[i].tokenType == Lexer.TOKEN_BASE.fn) {
						end = i-1;
						break;
					}
				}
				var tokens = [];
				for(var i = index+1;i < end;i++) {
					var t = this.parseToken(lexerResult.tokens[i],lexerResult,i);
					if(t == null) return null;
					i += t.tokens.length-1;
					tokens.push(t);
				}
				if(this.findFunction(token.image.replace(":","")) != null) throw new Error("Function already exists");
				return new TokenFunction(token,tokens);
			} else {
				var size = INSTR_SIZES[token.image.length]+1;
				var tokens = lexerResult.tokens.slice(index,index+size);
				return new TokenInstruction(tokens);
			}
		} catch(ex) {
			this.errors.push(ex);
			return null;
		}
		this.errors.push(new Error("Invalid token at "+token.startLine));
		return null;
	}
	parse(lexerResult,filename="console") {
		if(lexerResult.errors.length > 0) {
			this.errors = [];
			for(var error of lexerResult.errors) {
				this.errors.push(new Error(error.message,filename,error.line));
			}
			return this;
		}
		this.length = 0;
		this.tokens = [];
		for(var i = 0;i < lexerResult.tokens.length;i++) {
			var token = this.parseToken(lexerResult.tokens[i],lexerResult,i);
			if(token == null) break;
			i += token.length;
			this.tokens.push(token);
			this.length += token.length;
		}
		return this;
	}
}

module.exports = {
	Parser: Parser,
	parseInput: (text,filename) => {
		return new Parser().parse(Lexer.tokenize(text),filename);
	}
};
