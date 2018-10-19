const fs = require("fs");
const jints = require("jints");
const Parser = require("./parser");

class Assembler {
	constructor(opts = {}) {
		this.opts = opts;
		this.errors = [];
		this.instrs = null;
	}
	compileFile(path) {
		return this.compile(fs.readFileSync(path).toString());
	}
	compileFiles(files) {
		var str = [];
		for(var file of files) str.push(fs.readFileSync(file).toString());
		return this.compile(str.join("\n"));
	}
	compile(str) {
		var parser = Parser.parseInput(str);
		if(parser.errors.length > 0) {
			this.errors = [];
			for(var err of parser.errors) this.errors.push(err);
			return this;
		}
		var size = 0;
		for(var token of parser.tokens) {
			try {
				var instr = token.compile(parser);
				size += instr.length;
			} catch(ex) {
				this.errors.push(ex);
				return this;
			}
		}
		var instrs = Buffer.alloc(size);
		var index = 0;
		for(var token of parser.tokens) {
			try {
				var instr = token.compile(parser);
				for(var i = 0;i < instr.length;i++) instrs[index++] = instr[i];
			} catch(ex) {
				this.errors.push(ex);
				return this;
			}
		}
		this.instrs = instrs;
		return this;
	}
}
module.exports = Assembler;
