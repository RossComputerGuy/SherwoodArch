#!/usr/bin/env node

const HDDStorageDevice = require("../vm/hardware/devices/storage/hdd");
const X11Graphics2DDevice = require("../vm/hardware/devices/gfx/x11-2d");
const VirtualMachine = require("../vm");
const fs = require("fs");
const program = require("commander")
	.version("1.0.0")
	.usage("[options] <file ...>")
	.option("-c, --clockspeed <number>","Sets the CPU's clockspeed",1)
	.option("-f, --firmware <file>","Loads firmware from a file")
	.option("-m, --memory <number>","Sets the system physical RAM size",0x40000000)
	.option("-r, --ram <file>","Load and save RAM to/from file (Start vector is overrided by firmware)")
	.option("-d, --hdd <file...>","Add a HDD image/physical device, spliten by a comma",val => val.split(","))
	.option("-g, --graphics <string...>","Sets the graphical displays to use.",val => val.split(","))
	.parse(process.argv);

var vm = new VirtualMachine({
	clockspeed: program.clockspeed,
	ramSize: program.memory
});
if(program.graphics) {
	for(var gfx of program.graphics) {
		switch(gfx) {
			case "x11-2d":
				vm.mailbox.addDevice(new X11Graphics2DDevice())
				break;
			default: throw new Error("Unknown graphics display called: "+gfx);
		}
	}
}
if(program.hdd) {
	for(var drive of program.hdd) vm.mailbox.addDevice(new HDDStorageDevice(drive));
}
vm.reset();
process.stdin.setRawMode(true);
process.stdin.resume();
process.stdin.on("data",buff => {
	if(buff == "\u0003") vm.stop();
	vm.uart.emit("input",buff.toString());
});
vm.uart.on("output",(str) => process.stdout.write(str));
if(program.ram && fs.existsSync(program.ram)) vm.loadFirmware(program.ram);
if(program.firmware) vm.loadFirmware(program.firmware);
vm.on("cpu/stop",() => {
	if(program.ram) vm.dumpRAM(program.ram);
	vm.destroy();
	process.exit();
});
vm.start();
