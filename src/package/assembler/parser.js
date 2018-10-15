const jints = require("jints");
const Lexer = require("./lexer.js");
const vm = require("vm");

const REGISTERS = (() => {
	var r = ["flags","tmp","sp","ip","pc","cycle"];
	var bigr = ["data","index","addr","ptr"];
	for(var br of bigr) {
		for(var i = 0;i < 10;i++) r.push(br+i);
	}
	return r;
})();

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
	compileRegister(token) {
		var str = token.image.replace("%","");
		if(REGISTERS.indexOf(str) == -1) throw new Error(str+" is not a register");
		return REGISTERS.indexOf(str);
	}
	compileRRMM(a,b) {
		if(this.tokens[1].tokenType != this.tokens[3].tokenType != Lexer.TOKEN_BASE.register) throw new Error("Address and value parameters must be the same");
		if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.register) {
			instr[0] = a;
			instr[1] = this.compileRegister(this.tokens[1]);
			instr[2] = this.compileRegister(this.tokens[3]);
		} else if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.address) {
			instr[0] = b;
			instr[1] = this.compileAddress(this.tokens[1]);
			instr[2] = this.compileAddress(this.tokens[3]);
		} else throw new Error("Invalid parameter types");
	}
	compileFN(parser,token) {
		return 0xA0000000+parser.findFunctionOffset(token.image);
	}
	compile(parser) {
		var instr = [0,0,0];
		
		switch(this.name) {
			case "nop":
				instr[0] = 0;
				break;
			case "add":
				this.compileRRMM(1,2);
				break;
			case "sub":
				this.compileRRMM(3,4);
				break;
			case "mul":
				this.compileRRMM(5,6);
				break;
			case "div":
				this.compileRRMM(7,8);
				break;
			case "and":
				this.compileRRMM(9,10);
				break;
			case "or":
				this.compileRRMM(11,12);
				break;
			case "xor":
				this.compileRRMM(13,14);
				break;
			case "nor":
				this.compileRRMM(15,16);
				break;
			case "nand":
				this.compileRRMM(17,18);
				break;
			case "lshift":
				this.compileRRMM(19,20);
				break;
			case "rshift":
				this.compileRRMM(21,22);
				break;
			case "cmp":
				this.compileRRMM(23,24);
				break;
			case "jit":
				if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.register) {
					instr[0] = 25;
					instr[1] = this.compileRegister(this.tokens[1]);
				} else if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.address) {
					instr[0] = 26;
					instr[1] = this.compileAddress(this.tokens[1]);
				} else if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.integer) {
					instr[0] = 27;
					instr[1] = parseInt(this.tokens[1].image);
				} else if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.identifier) {
					instr[0] = 27;
					instr[1] = this.compileFN(parser,this.tokens[1]);
				} else throw new Error("Invalid address parameter");
				break;
			case "jmp":
				if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.register) {
					instr[0] = 28;
					instr[1] = this.compileRegister(this.tokens[1]);
				} else if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.address) {
					instr[0] = 29;
					instr[1] = this.compileAddress(this.tokens[1]);
				} else if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.integer) {
					instr[0] = 30;
					instr[1] = parseInt(this.tokens[1].image);
				} else if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.identifier) {
					instr[0] = 30;
					instr[1] = this.compileFN(parser,this.tokens[1]);
				} else throw new Error("Invalid address parameter");
				break;
			case "call":
				if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.register) {
					instr[0] = 31;
					instr[1] = this.compileRegister(this.tokens[1]);
				} else if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.address) {
					instr[0] = 32;
					instr[1] = this.compileAddress(this.tokens[1]);
				} else if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.integer) {
					instr[0] = 33;
					instr[1] = parseInt(this.tokens[1].image);
				} else if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.identifier) {
					instr[0] = 33;
					instr[1] = this.compileFN(parser,this.tokens[1]);
				} else throw new Error("Invalid address parameter");
				break;
			case "ret":
				instr[0] = 34;
				break;
			case "push":
				if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.register) {
					instr[0] = 35;
					instr[1] = this.compileRegister(this.tokens[1]);
				} else if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.address) {
					instr[0] = 36;
					instr[1] = this.compileAddress(this.tokens[1]);
				} else throw new Error("Invalid address parameter");
				break;
			case "pop":
				if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.register) {
					instr[0] = 37;
					instr[1] = this.compileRegister(this.tokens[1]);
				} else if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.address) {
					instr[0] = 38;
					instr[1] = this.compileAddress(this.tokens[1]);
				} else throw new Error("Invalid address parameter");
				break;
			case "mov":
				if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.register && this.tokens[3].tokenType == Lexer.TOKEN_BASE.register) {
					instr[0] = 39;
					instr[1] = this.compileRegister(this.tokens[1]);
					instr[2] = this.compileRegister(this.tokens[3]);
				} else if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.register && this.tokens[3].tokenType == Lexer.TOKEN_BASE.address) {
					instr[0] = 40;
					instr[1] = this.compileRegister(this.tokens[1]);
					instr[2] = this.compileAddress(this.tokens[3]);
				} else if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.address && this.tokens[3].tokenType == Lexer.TOKEN_BASE.register) {
					instr[0] = 41;
					instr[1] = this.compileAddress(this.tokens[1]);
					instr[2] = this.compileRegister(this.tokens[3]);
				} else if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.address && this.tokens[1].tokenType == Lexer.TOKEN_BASE.address) {
					instr[0] = 42;
					instr[1] = this.compileAddress(this.tokens[1]);
					instr[2] = this.compileAddress(this.tokens[3]);
				} else throw new Error("Invalid parameters");
				break;
			case "sto":
				if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.register) {
					instr[0] = 43;
					instr[1] = this.compileRegister(this.tokens[1]);
				} else if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.address) {
					instr[0] = 44;
					instr[1] = this.compileAddress(this.tokens[1]);
				} else throw new Error("Invalid address parameter");
				
				if(this.tokens[3].tokenType == Lexer.TOKEN_BASE.integer) {
					instr[2] = parseInt(this.tokens[3].image);
				} else if(this.tokens[3].tokenType == Lexer.TOKEN_BASE.identifier) {
					instr[2] = this.compileFN(parser,this.tokens[1]);
				} else if(this.tokens[3].tokenType == Lexer.TOKEN_BASE.char) {
					var sb = { result: this.tokens[3].image.slice(1,-1) };
					vm.runInNewContext("result = "+this.tokens[3].image+";",sb,"assembler.js");
					instr[2] = Buffer.from(sb.result)[0];
				} else throw new Error("Invalid data parameter");
				break;
			case "int":
				if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.register) {
					instr[0] = 45;
					instr[1] = this.compileRegister(this.tokens[1]);
				} else if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.address) {
					instr[0] = 46;
					instr[1] = this.compileAddress(this.tokens[1]);
				} else if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.integer) {
					instr[0] = 47;
					instr[1] = parseInt(this.tokens[1].image);
				} else throw new Error("Invalid address parameter");
				break;
			case "iret":
				instr[0] = 48;
				break;
			case "lditbl":
				if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.register) {
					instr[0] = 49;
					instr[1] = this.compileRegister(this.tokens[1]);
				} else if(this.tokens[1].tokenType == Lexer.TOKEN_BASE.address) {
					instr[0] = 50;
					instr[1] = this.compileAddress(this.tokens[1]);
				} else throw new Error("Invalid address parameter");
				break;
			case "hlt":
				instr[0] = 51;
				break;
			case "rst":
				instr[0] = 52;
				break;
			default: throw new Error("Invalid instruction");
		}
		
		for(var i = 0;i < instr.length;i++) {
			if(!(instr[i] instanceof jints.UInt64)) instr[i] = new jints.UInt64(instr[i]);
		}
		for(var i = 0;i < instr.length;i++) instr[i] = instr[i].toArray();
		var buff = Buffer.alloc(8*3);
		var index = 0;
		for(var i = 0;i < instr.length;i++) {
			for(var x = 0;x < 8;x++) buff[index++] = instr[i][x];
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
			if(token.tokenType == Lexer.TOKEN_INSTRUCTIONS.nop) return new TokenInstruction([token]);
			if(token.tokenType == Lexer.TOKEN_INSTRUCTIONS.add) return new TokenInstruction([token,lexerResult.tokens[index+1],lexerResult.tokens[index+2],lexerResult.tokens[index+3]]);
			if(token.tokenType == Lexer.TOKEN_INSTRUCTIONS.sub) return new TokenInstruction([token,lexerResult.tokens[index+1],lexerResult.tokens[index+2],lexerResult.tokens[index+3]]);
			if(token.tokenType == Lexer.TOKEN_INSTRUCTIONS.mul) return new TokenInstruction([token,lexerResult.tokens[index+1],lexerResult.tokens[index+2],lexerResult.tokens[index+3]]);
			if(token.tokenType == Lexer.TOKEN_INSTRUCTIONS.div) return new TokenInstruction([token,lexerResult.tokens[index+1],lexerResult.tokens[index+2],lexerResult.tokens[index+3]]);
			if(token.tokenType == Lexer.TOKEN_INSTRUCTIONS.and) return new TokenInstruction([token,lexerResult.tokens[index+1],lexerResult.tokens[index+2],lexerResult.tokens[index+3]]);
			if(token.tokenType == Lexer.TOKEN_INSTRUCTIONS.or) return new TokenInstruction([token,lexerResult.tokens[index+1],lexerResult.tokens[index+2],lexerResult.tokens[index+3]]);
			if(token.tokenType == Lexer.TOKEN_INSTRUCTIONS.xor) return new TokenInstruction([token,lexerResult.tokens[index+1],lexerResult.tokens[index+2],lexerResult.tokens[index+3]]);
			if(token.tokenType == Lexer.TOKEN_INSTRUCTIONS.nor) return new TokenInstruction([token,lexerResult.tokens[index+1],lexerResult.tokens[index+2],lexerResult.tokens[index+3]]);
			if(token.tokenType == Lexer.TOKEN_INSTRUCTIONS.nand) return new TokenInstruction([token,lexerResult.tokens[index+1],lexerResult.tokens[index+2],lexerResult.tokens[index+3]]);
			if(token.tokenType == Lexer.TOKEN_INSTRUCTIONS.lshift) return new TokenInstruction([token,lexerResult.tokens[index+1],lexerResult.tokens[index+2],lexerResult.tokens[index+3]]);
			if(token.tokenType == Lexer.TOKEN_INSTRUCTIONS.rshift) return new TokenInstruction([token,lexerResult.tokens[index+1],lexerResult.tokens[index+2],lexerResult.tokens[index+3]]);
			if(token.tokenType == Lexer.TOKEN_INSTRUCTIONS.cmp) return new TokenInstruction([token,lexerResult.tokens[index+1],lexerResult.tokens[index+2],lexerResult.tokens[index+3]]);
			if(token.tokenType == Lexer.TOKEN_INSTRUCTIONS.jit) return new TokenInstruction([token,lexerResult.tokens[index+1]]);
			if(token.tokenType == Lexer.TOKEN_INSTRUCTIONS.jmp) return new TokenInstruction([token,lexerResult.tokens[index+1]]);
			if(token.tokenType == Lexer.TOKEN_INSTRUCTIONS.call) return new TokenInstruction([token,lexerResult.tokens[index+1]]);
			if(token.tokenType == Lexer.TOKEN_INSTRUCTIONS.ret) return new TokenInstruction([token]);
			if(token.tokenType == Lexer.TOKEN_INSTRUCTIONS.push) return new TokenInstruction([token,lexerResult.tokens[index+1]]);
			if(token.tokenType == Lexer.TOKEN_INSTRUCTIONS.pop) return new TokenInstruction([token,lexerResult.tokens[index+1]]);
			if(token.tokenType == Lexer.TOKEN_INSTRUCTIONS.mov) return new TokenInstruction([token,lexerResult.tokens[index+1],lexerResult.tokens[index+2],lexerResult.tokens[index+3]]);
			if(token.tokenType == Lexer.TOKEN_INSTRUCTIONS.sto) return new TokenInstruction([token,lexerResult.tokens[index+1],lexerResult.tokens[index+2],lexerResult.tokens[index+3]]);
			if(token.tokenType == Lexer.TOKEN_INSTRUCTIONS.int) return new TokenInstruction([token,lexerResult.tokens[index+1]]);
			if(token.tokenType == Lexer.TOKEN_INSTRUCTIONS.iret) return new TokenInstruction([token]);
			if(token.tokenType == Lexer.TOKEN_INSTRUCTIONS.lditbl) return new TokenInstruction([token,lexerResult.tokens[index+1]]);
			if(token.tokenType == Lexer.TOKEN_INSTRUCTIONS.hlt) return new TokenInstruction([token]);
			if(token.tokenType == Lexer.TOKEN_INSTRUCTIONS.rst) return new TokenInstruction([token,lexerResult.tokens[index+1]]);
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
