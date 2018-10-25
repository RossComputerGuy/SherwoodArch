const assert = require("assert");
const SherwoodArch = require("../");

describe("SherwoodArch",function() {
	describe("#VirtualMachine",function() {
		describe("UART writing",function() {
			it("Sending character to the UART",function() {
				var vm = new SherwoodArch.VirtualMachine();
				vm.reset();
				vm.uart.on("output",(str) => assert.equal(str,"H"));
				vm.ioctl.ram[0] = (23 << 40) | (2 << 48);
				vm.ioctl.ram[1] = 0x10000013;
				vm.ioctl.ram[2] = 72;
				vm.cpu.cores[0].running = true;
				while(vm.cpu.cores[0].running) vm.cycle();
				vm.destroy();
			});
		});
		describe("Math",function() {
			it("Mathematical stuff",function() {
				var vm = new SherwoodArch.VirtualMachine();
				vm.reset();
				
				vm.ioctl.ram[0] = (1 << 40) | (1 << 48);
				vm.ioctl.ram[1] = 0xA00000FF;
				vm.ioctl.ram[2] = 0xA00000FF;
				
				vm.ioctl.ram[3] = (2 << 40) | (1 << 48);
				vm.ioctl.ram[4] = 0xA00000FF;
				vm.ioctl.ram[5] = 0xA00000FF;
				
				vm.ioctl.ram[6] = (13 << 40) | (1 << 48);
				vm.ioctl.ram[7] = 0xA00000FE;
				vm.ioctl.ram[8] = 0xA00000FF;
				
				vm.ioctl.ram[254] = 0;
				vm.ioctl.ram[255] = 1;
				vm.cpu.cores[0].running = true;
				while(vm.cpu.cores[0].running) vm.cycle();
				assert.equal(vm.cpu.cores[0].regs.tmp[0],1);
				vm.destroy();
			});
		});
		describe("Interrupting",function() {
			it("Should throw an exception when trying to run an interrupt with no IVT loaded",function() {
				var vm = new SherwoodArch.VirtualMachine();
				vm.reset();
				
				vm.ioctl.ram[0] = (24 << 40) | (2 << 48);
				vm.ioctl.ram[1] = 1;
				vm.ioctl.ram[2] = 0;
				
				vm.cpu.cores[0].running = true;
				try {
					while(vm.cpu.cores[0].running) vm.cycle();
				} catch(ex) {
					assert.equal(ex.message,"SAVM_ERROR_DOUBLE_FAULT: IVT is not loaded");
				}
				vm.destroy();
			});
		});
		describe("CRC",function() {
			it("CRC32 calculation testing",function() {
				var vm = new SherwoodArch.VirtualMachine();
				vm.reset();
				
				vm.ioctl.ram[0] = (28 << 40) | (2 << 48);
				vm.ioctl.ram[1] = 0xA00000FE;
				vm.ioctl.ram[2] = 1;
				
				vm.ioctl.ram[254] = 1;
				vm.cpu.cores[0].running = true;
				while(vm.cpu.cores[0].running) vm.cycle();
				assert.equal(vm.cpu.cores[0].regs.tmp,2768625435);
				vm.destroy();
			});
		});
	});
});
