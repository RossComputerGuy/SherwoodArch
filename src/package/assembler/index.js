const fs = require("fs");
const jints = require("jints");
const Parser = require("./parser");

class Assembler {
	constructor(opts = {}) {
		this.opts = opts;
		this.errors = [];
		this.data = null;
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
	compileInstrs(str) {
		var parser = Parser.parseInput(str);
		if(parser.errors.length > 0) {
			this.errors = [];
			for(var err of parser.errors) this.errors.push(err);
			return this;
		}
		var size = 0;
		for(var token of parser.tokens) {
			if(token.type != "data") {
				try {
					var instr = token.compile(parser);
					size += instr.length;
				} catch(ex) {
					this.errors.push(ex);
					return this;
				}
			}
		}
		var instrs = Buffer.alloc(size);
		var index = 0;
		for(var token of parser.tokens) {
			if(token.type != "data") {
				try {
					var instr = token.compile(parser);
					for(var i = 0;i < instr.length;i++) instrs[index++] = instr[i];
				} catch(ex) {
					this.errors.push(ex);
					return this;
				}
			}
		}
		this.instrs = instrs;
		return this;
	}
	compileData(str) {
		var parser = Parser.parseInput(str);
		if(parser.errors.length > 0) {
			this.errors = [];
			for(var err of parser.errors) this.errors.push(err);
			return this;
		}
		var size = 0;
		for(var token of parser.tokens) {
			if(token.type == "data") {
				try {
					var instr = token.compile(parser);
					size += instr.length;
				} catch(ex) {
					this.errors.push(ex);
					return this;
				}
			}
		}
		var data = Buffer.alloc(size);
		var index = 0;
		for(var token of parser.tokens) {
			if(token.type == "data") {
				try {
					var instr = token.compile(parser);
					for(var i = 0;i < instr.length;i++) data[index++] = instr[i];
				} catch(ex) {
					this.errors.push(ex);
					return this;
				}
			}
		}
		this.data = data;
		return this;
	}
	compile(str) {
		this.compileInstrs(str);
		if(this.errors.length > 0) return this;
		this.compileData(str);
		if(this.errors.length > 0) return this;
		var buff = Buffer.alloc(this.instrs.length+3+this.data.length);
		for(var i = 0;i < this.instrs.length;i++) buff[i] = this.instrs[i];
		for(var i = 0;i < 3;i++) buff[this.instrs.length+i] = 0;
		for(var i = 0;i < this.data.length;i++) buff[this.instrs.length+3+i] = this.data[i];
		this.prog = buff;
		return this;
	}
}
module.exports = Assembler;
