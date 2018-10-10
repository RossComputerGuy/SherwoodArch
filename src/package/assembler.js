const fs = require("fs");
const vm = require("vm");

const INSTRUCTION_SET = [
	"nop","addr","addm","subr","subm","mulr","mulm","divr","divm",
	"andr","andm","orr","orm","xorr","xorm","norr","norm","nandr","nandm","lshiftr","lshiftm","rshiftr","rshiftm",
	"cmpr","cmpm","jitr","jitm","jit","jmpr","jmpm","jmp","callr","callm","call","ret",
	"pushr","pushm","popr","popm","movrr","movrm","movmr","movmm","stor","stom",
	"intr","intm","int","iret","lditblr","lditblm","hlt"
];

function compileOpcode(str,opts) {
	if(str[0]+str[1] == "0x") return parseInt(str.slice(2),16);
	if(str[0]+str[1] == "0b") return parseInt(str.slice(2),2);
	if(str[0] == "\'" && str[str.length-1] == "\'") {
		if(str.length != 3 && str.length != 4) throw new Error("Failed to compile character");
		if(str.length == 4 && str[1] != "\\") throw new Error("Failed to compile character");
		if(str[1]) {
			var sb = { result: str.slice(1,-1) };
			vm.runInNewContext("result = \""+str+"\";",sb,"assembler.js");
			str = sb.result;
		}
		if(str[0] == '\'' && str[str.length-1] == '\'') str = str.slice(1,-1);
		console.log(str);
		return Buffer.from([str])[0];
	}
	return parseInt(str);
}

function compileLine(str,opts) {
	str = str.split(", ").join(",").split(",").join(" ");
	var opcodes = str.split(" ",3);
	opcodes[0] = opcodes[0].toLowerCase();
	var instr = new Float64Array(3);
	if(INSTRUCTION_SET.indexOf(opcodes[0]) == -1) throw new Error("No instruction called \""+opcodes[0]+"\" exists");
	instr[0] = INSTRUCTION_SET.indexOf(opcodes[0]);
	for(var i = 1;i < opcodes.length;i++) instr[i] = compileOpcode(opcodes[i],opts);
	if(opts.verbose) console.log("Compiled Line: "+instr[0]+" "+instr[1]+", "+instr[2]);
	return instr;
}

function compileString(str,opts) {
	var lines = str.split("\n");
	var instrs = new Float64Array(lines.length*3);
	var index = 0;
	for(var line of lines) {
		if(line.length > 0) {
			var instr = compileLine(line,opts);
			for(var i = 0;i < 3;i++) instrs[index++] = instr[i];
		}
	}
	return instrs;
}

function compileFile(path,opts) {
	return compileString(fs.readFileSync(path).toString(),opts);
}

function compile(str,opts) {
	if(!opts) opts = { verbose: false };
	if(typeof(opts.verbose) == "undefined") opts.verbose = false;
	var isFile = fs.existsSync(str);
	if(isFile) return Buffer.from(compileFile(str,opts));
	return Buffer.from(compileString(str,opts));
}

module.exports = compile;
