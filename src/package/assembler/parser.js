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

const INSTR_ADDRMODE = {
	"DEFAULT": 0,
	"REG": 0,
	"ADDR": 1,
	"RAW": 2
};

const INSTR_SIZES = {
	"nop": 0,
	"add": 3,
	"sub": 3,
	"mul": 3,
	"div": 3,
	"and": 3,
	"or": 3,
	"xor": 3,
	"nor": 3,
	"nand": 3,
	"mod": 3,
	"lshift": 3,
	"rshift": 3,
	"cmp": 3,
	"grtn": 3,
	"lstn": 3,
	"jit": 3,
	"jmp": 1,
	"call": 1,
	"ret": 0,
	"push": 1,
	"pop": 1,
	"mov": 3,
	"int": 1,
	"iret": 1,
	"lditbl": 1,
	"rst": 0,
	"crc": 3
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
	compileData(parser,token) {
		return 0xA0000000+parser.findDataOffset(token.image);
	}
	compileParam(parser,token) {
		if(token.tokenType == Lexer.TOKEN_BASE.register) return this.compileRegister(token);
		if(token.tokenType == Lexer.TOKEN_BASE.identifier) {
			if(parser.findFunction(token.image) == null) throw new Error("No such function called \""+token.image+"\" exists");
			else return this.compileFN(parser,token);
			if(parser.findData(token.image) == null) throw new Error("No such data called \""+token.image+"\" exists");
			else return this.compileData(parser,token);
			throw new Error("No such token called \""+token.image+"\" exists");
		}
		if(token.tokenType == Lexer.TOKEN_BASE.address) return this.compileAddress(token);
		if(token.tokenType == Lexer.TOKEN_BASE.char) return this.compileChar(token);
		if(token.tokenType == Lexer.TOKEN_BASE.integer) {
			if(token.image.slice(0,2) == "0b") return parseInt(token.image.replace("0b",""),2);
			if(token.image.slice(0,2) == "0x") return parseInt(token.image.replace("0x",""),16);
			return parseInt(token.image);
		}
	}
	compile(parser) {
		var opcodes = [0,0,0];
		
		if(this.tokens.length >= 2) {
			if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.register) opcodes[0] |= (INSTR_ADDRMODE["REG"] << 48);
			if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.address) opcodes[0] |= (INSTR_ADDRMODE["ADDR"] << 48);
			if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.integer
				|| this.tokens[1].tokenType == Lexer.TOKEN_BASE.char
				|| this.tokens[1].tokenType == Lexer.TOKEN_BASE.identifier) opcodes[0] |= (INSTR_ADDRMODE["RAW"] << 48);
			
			opcodes[1] = this.compileParam(parser,this.tokens[1]);
		}
		
		if(this.tokens.length == 4) opcodes[2] = this.compileParam(parser,this.tokens[3]);
		
		switch(this.name) {
			case "nop":
				opcodes[0] |= (0 << 40);
				break;
			case "add":
				opcodes[0] |= (1 << 40);
				break;
			case "sub":
				opcodes[0] |= (2 << 40);
				break;
			case "mul":
				opcodes[0] |= (3 << 40);
				break;
			case "div":
				opcodes[0] |= (4 << 40);
				break;
			case "and":
				opcodes[0] |= (5 << 40);
				break;
			case "or":
				opcodes[0] |= (6 << 40);
				break;
			case "xor":
				opcodes[0] |= (7 << 40);
				break;
			case "nor":
				opcodes[0] |= (8 << 40);
				break;
			case "nand":
				opcodes[0] |= (9 << 40);
				break;
			case "mod":
				opcodes[0] |= (10 << 40);
				break;
			case "lshift":
				opcodes[0] |= (11 << 40);
				break;
			case "rshift":
				opcodes[0] |= (12 << 40);
				break;
			case "cmp":
				opcodes[0] |= (13 << 40);
				break;
			case "grtn":
				opcodes[0] |= (14 << 40);
				break;
			case "lstn":
				opcodes[0] |= (15 << 40);
				break;
			case "jit":
				opcodes[0] |= (16 << 40);
				break;
			case "jmp":
				opcodes[0] |= (17 << 40);
				break;
			case "call":
				opcodes[0] |= (18 << 40);
				break;
			case "ret":
				opcodes[0] |= (19 << 40);
				break;
			case "push":
				opcodes[0] |= (20 << 40);
				break;
			case "pop":
				opcodes[0] |= (21 << 40);
				break;
			case "mov":
				if(this.tokens[3].tokenType == Lexer.TOKEN_BASE.register) opcodes[0] |= (INSTR_ADDRMODE["REG"] << 48);
				if(this.tokens[3].tokenType == Lexer.TOKEN_BASE.identifier
					|| this.tokens[3].tokenType == Lexer.TOKEN_BASE.address) opcodes[0] |= (INSTR_ADDRMODE["ADDR"] << 48);
				if(this.tokens[3].tokenType == Lexer.TOKEN_BASE.integer
					|| this.tokens[3].tokenType == Lexer.TOKEN_BASE.char) opcodes[0] |= (INSTR_ADDRMODE["RAW"] << 48);
				if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.register) opcodes[0] |= (22 << 40);
				else if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.address || this.tokens[1].tokenType == Lexer.TOKEN_BASE.identifier) opcodes[0] |= (23 << 40);
				else throw new Error("Address parameter is not a register or memory address");
				break;
			case "int":
				opcodes[0] |= (24 << 40);
				break;
			case "iret":
				opcodes[0] |= (25 << 40);
				break;
			case "lditbl":
				opcodes[0] |= (26 << 40);
				break;
			case "rst":
				opcodes[0] |= (27 << 40);
				break;
			case "crc":
				opcodes[0] |= (28 << 40);
				break;
			default: throw new Error("Invalid instruction");
		}
		for(var i = 0;i < opcodes.length;i++) {
			if(!(opcodes[i] instanceof jints.UInt64)) opcodes[i] = new jints.UInt64(opcodes[i]);
		}
		for(var i = 0;i < opcodes.length;i++) opcodes[i] = opcodes[i].toArray();
		var buff = Buffer.alloc(8*3);
		var index = 0;
		for(var i = 0;i < opcodes.length;i++) {
			for(var x = 0;x < 8;x++) buff[index++] = opcodes[i][x];
		}
		return buff;
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

class TokenData extends Token {
	constructor(dataToken,token) {
		super("data");
		this.token = token;
		this.tokens = [dataToken,token];
		this.name = dataToken.image.replace(".","");
		this.length = 1;
		switch(token.tokenType) {
			case Lexer.TOKEN_BASE.char:
			case Lexer.TOKEN_BASE.string:
				var sb = { result: token.image.slice(1,-1) };
				vm.runInNewContext("result = "+token.image+";",sb,"assembler.js");
				this.value = Buffer.from(sb.result);
				this.length = this.value.length;
				break;
			case Lexer.TOKEN_BASE.integer:
				if(token.image.slice(0,2) == "0b") this.value = parseInt(token.image.replace("0b",""),2);
				else if(token.image.slice(0,2) == "0x") this.value = parseInt(token.image.replace("0x",""),16);
				else this.value = new jints.UInt64(parseInt(token.image)).toArray();
				this.length = 8;
				break;
			default:
				throw new Error("Unknown token type: "+token.tokenType.tokenName);
		}
		this.length = this.value.length;
	}
	compile(parser) {
		var data = Buffer.alloc(this.length);
		for(var i = 0;i < this.value.length;i++) data[i] = this.value[i];
		return data;
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
	findDataOffset(name) {
		var offset = 0;
		for(var token of this.tokens) {
			if(token.type == "data" && token.name == name) return offset;
			else offset += token.length;
		}
		return null;
	}
	findData(name) {
		for(var token of this.tokens) {
			if(token.type == "data" && token.name == name) return token;
		}
		return null;
	}
	parseToken(token,lexerResult,index) {
		try {
			if(token.tokenType == Lexer.TOKEN_BASE.data) {
				return new TokenData(token,lexerResult.tokens[index+1]);
			} else if(token.tokenType == Lexer.TOKEN_BASE.fn) {
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
				if(typeof(INSTR_SIZES[token.image]) == "undefined") throw new Error("No such instruction called \""+token.image+"\" exists");
				var size = INSTR_SIZES[token.image]+1;
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
