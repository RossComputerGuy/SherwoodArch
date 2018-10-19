#!/usr/bin/env node

const Assembler = require("../assembler");
const fs = require("fs");
const program = require("commander")
	.version("1.0.0")
	.usage("[options] <file ...>")
	.option("-v, --verbose","Produce verbose output")
	.option("-o, --output <file>","Output to FILE instead of a.out","a.out")
	.parse(process.argv);

var asm = new Assembler();
var program = asm.compileFiles(program.args[i]).instrs;
for(var err of asm.errors) {
	console.error(err);
}
if(asm.errors.length > 0) process.exit(1);
if(program.verbose) console.log("Total Size of Binary: "+program.length+" Bytes");
if(program.verbose) console.log("Writing to "+program.output);
fs.writeFileSync(program.output,Buffer.from(program));
