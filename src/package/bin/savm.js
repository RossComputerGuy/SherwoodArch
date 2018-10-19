#!/usr/bin/env node

const VirtualMachine = require("../vm");
const fs = require("fs");
const program = require("commander")
	.version("1.0.0")
	.usage("[options] <file ...>")
	.option("-c, --clockspeed <number>","Sets the CPU's clockspeed",1)
	.option("-f, --firmware <file>","Loads firmware from a file")
	.option("-m, --memory <number>","Sets the system physical RAM size",0x40000000)
	.option("-r, --ram <file>","Load and save RAM to/from file (Start vector is overrided by firmware)")
	.parse(process.argv);

var vm = new VirtualMachine({
	clockspeed: program.clockspeed,
	ramSize: program.memory
});
vm.reset();
process.stdin.setRawMode(true);
process.stdin.resume();
process.stdin.on("data",buff => {
	if(buff == "\u0003") vm.stop();
	vm.uart.emit("input",buff.toString());
});
vm.uart.on("output",(str) => process.stdout.write(str));
if(program.ram && fs.existsSync(program.ram)) vm.loadFile(0xA0000000,program.ram);
if(program.firmware) vm.loadFirmware(program.firmware);
vm.on("cpu/stop",() => {
	if(program.ram) fs.writeFileSync(program.ram,vm.ioctl.ram);
	vm.destroy();
	process.exit();
});
vm.start();
