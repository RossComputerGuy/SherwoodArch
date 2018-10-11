const fs = require("fs");
const jints = require("jints");
const utils = require("./utils");
const vm = require("vm");

const INSTRUCTION_SET = [
	"nop","addr","addm","subr","subm","mulr","mulm","divr","divm",
	"andr","andm","orr","orm","xorr","xorm","norr","norm","nandr","nandm","lshiftr","lshiftm","rshiftr","rshiftm",
	"cmpr","cmpm","jitr","jitm","jit","jmpr","jmpm","jmp","callr","callm","call","ret",
	"pushr","pushm","popr","popm","movrr","movrm","movmr","movmm","stor","stom",
	"intr","intm","int","iret","lditblr","lditblm","hlt"
];

const REGISTERS = (() => {
	var r = ["flags","tmp","sp","ip","pc","cycle"];
	var bigr = ["data","index","addr","ptr"];
	for(var br of bigr) {
		for(var i = 0;i < 10;i++) r.push(br+i);
	}
	return r;
})();

function compileOpcode(str,opts) {
	if(str[0]+str[1] == "0x") return parseInt(str.slice(2),16);
	if(str[0]+str[1] == "0b") return parseInt(str.slice(2),2);
	if(str[0] == "%") {
		str = str.replace("%","");
		if(REGISTERS.indexOf(str) == -1) throw new Error("Invalid register "+str);
		return REGISTERS.indexOf(str);
	}
	if(str[0] == "\'" && str[str.length-1] == "\'") {
		if(str.length != 3 && str.length != 4) throw new Error("Failed to compile character");
		if(str.length == 4 && str[1] != "\\") throw new Error("Failed to compile character");
		if(str[1]) {
			var sb = { result: str.slice(1,-1) };
			vm.runInNewContext("result = \""+str+"\";",sb,"assembler.js");
			str = sb.result;
		}
		if(str[0] == '\'' && str[str.length-1] == '\'') str = str.slice(1,-1);
		return Buffer.from(str)[0];
	}
	return parseInt(str);
}

function compileLine(str,opts) {
	str = str.split(", ",2).join(",").split(",",2).join(" ");
	var opcodes = str.split(" ",3);
	opcodes[0] = opcodes[0].toLowerCase();
	var instr = [0,0,0];
	if(INSTRUCTION_SET.indexOf(opcodes[0]) == -1) throw new Error("No instruction called \""+opcodes[0]+"\" exists");
	instr[0] = new jints.UInt64(INSTRUCTION_SET.indexOf(opcodes[0]));
	for(var i = 1;i < opcodes.length;i++) instr[i] = compileOpcode(opcodes[i],opts);
	if(opts.verbose) console.log("Compiled Line: "+instr[0]+" "+instr[1]+", "+instr[2]);
	for(var i = 0;i < instr.length;i++) {
		if(!(instr[i] instanceof jints.UInt64)) instr[i] = new jints.UInt64(instr[i]);
	}
	for(var i = 0;i < instr.length;i++) instr[i] = instr[i].toArray();
	var buff = Buffer.alloc(3*8);
	var index = 0;
	for(var i = 0;i < instr.length;i++) {
		for(var x = 0;x < 8;x++) buff[index++] = instr[i][x];
	}
	return buff;
}

function compileString(str,opts) {
	var lines = str.split("\n");
	for(var i = 0;i < lines.length;i++) {
		if(lines[i].length <= 1 || lines[i][0] == "#") lines.splice(i,1);
	}
	var instrs = Buffer.alloc((lines.length*3)*8);
	instrs.fill(0);
	var index = 0;
	for(var line of lines) {
		var instr = compileLine(line,opts);
		for(var i = 0;i < 8*3;i++) instrs[index++] = instr[i];
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
