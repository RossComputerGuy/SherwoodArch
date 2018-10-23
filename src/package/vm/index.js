const EventEmitter = require("events");
const fs = require("fs");
const jints = require("jints");
const Mailbox = require("./hardware/mailbox");
const RTC = require("./hardware/rtc");
const UART = require("./hardware/uart");

const CPU_REG_FLAG_INTR = (1 << 0);
const CPU_REG_FLAG_ENIRQ = (1 << 1);
const CPU_REG_FLAG_PAGING = (1 << 2);
const CPU_REG_FLAG_PRIV_KERN = (1 << 3);
const CPU_REG_FLAG_PRIV_USER = (1 << 4);

const CPU_PAGING_PERM_KERN = (1 << 0);
const CPU_PAGING_PERM_USER = (1 << 1);

const CPU_INT = {
	"STACK_OVERFLOW": 0,
	"FAULT": 1,
	"BADADDR": 2,
	"DIVBYZERO": 3,
	"BADINSTR": 4,
	"BADPERM": 5,
	"TIMER": 6,
	"MAILBOX": 7,
	"SYSCALL": 8
};

const INSTR_ADDRMODE = {
	"DEFAULT": 0,
	"REG": 0,
	"ADDR": 1,
	"RAW": 2
};

const INSTR_FLAG_SIGNED = (1 << 0);
const INSTR_FLAG_UNSIGNED = ~INSTR_FLAG_SIGNED;

const IO_IOCTL_BASE = 0x10000016;
const IO_IOCTL_SIZE = 0x00000005;
const IO_IOCTL_END = IO_IOCTL_BASE+IO_IOCTL_SIZE;

const IO_RAM_BASE = 0xA0000000;
const IO_RAM_SIZE = 0x40000000;
const IO_RAM_END = IO_RAM_BASE+IO_RAM_SIZE;

const BITS_EXTRACT = (value,begin,size) => ((value >> begin) & (1 << ((begin+size)-begin))-1);

function setupRegisters() {
	return {
		flags: new Float64Array(1),
		tmp: new Float64Array(1),
		sp: new Float64Array(1),
		ip: new Float64Array(1),
		pc: new Float64Array(1),
		cycle: new Float64Array(1),
		data: new Float64Array(10),
		index: new Float64Array(10),
		addr: new Float64Array(10),
		ptr: new Float64Array(10)
	};
}

function setupCPUCore() {
	return {
		regs: setupRegisters(),
		iregs: setupRegisters(),
		
		stack: new Float64Array(20),
		ivt: new Float64Array(6),
		intr: new Float64Array(1),
		running: false,
		irqs: []
	};
}

class VirtualMachine extends EventEmitter {
	constructor(opts = {}) {
		super();
		this.cpu = {
			clockspeed: opts.clockspeed || 1,
			currentCore: 0,
			cores: new Array(opts.cores || 4)
		};
		this.ioctl = {
			ram: new Float64Array(opts.ramSize || IO_RAM_SIZE),
			mmap: [],
			pgdir: null,
			selectedCore: 0
		};
		this.opts = opts;
		
		this.mailbox = new Mailbox();
		this.rtc = new RTC();
		this.uart = new UART();
	}
	destroy() {
		if(this.mailbox) this.mailbox.destroy(this);
		if(this.rtc) this.rtc.destroy(this);
		if(this.uart) this.uart.destroy(this);
		this.emit("destroy");
	}
	reset() {
		/* (Re)initialize the RAM */
		if(this.ioctl.ram) delete this.ioctl.ram;
		this.ioctl.ram = new Float64Array(this.opts.ramSize || IO_RAM_SIZE);
		
		/* (Re)initialize the memory map in the IO Controller */
		if(this.ioctl.mmap) delete this.ioctl.mmap;
		this.ioctl.mmap = [];
		this.mmap(IO_IOCTL_BASE,IO_IOCTL_END,i => {
			switch(i) {
				case 0: return this.ioctl.ram.length;
				case 2: return this.cpu.cores.length;
				case 3: return this.cpu.currentCore;
				case 4: return this.ioctl.selectedCore;
			}
		},(i,v) => {
			switch(i) {
				case 1:
					if(this.cpu.cores[this.cpu.currentCore].regs.flags[0] & CPU_REG_FLAG_PRIV_USER) return this.intr(CPU_INT["BADPERM"]);
					this.ioctl.pgdir = {};
					this.ioctl.pgdir.tables = new Array(this.read(addr));
					for(var i = 0;i < this.ioctl.pgdir.tables.length;i++) {
						this.ioctl.pgdir.tables[i] = {};
						this.ioctl.pgdir.tables[i].page = new Array(this.read(addr));
						for(var x = 0;x < this.ioctl.pgdir.tables[i].page.length;x++) {
							var tmp = this.read(addr);
							
							this.ioctl.pgdir.tables[i].page[x] = {};
							this.ioctl.pgdir.tables[i].page[x].flags = tmp & 0xffffffff;
							this.ioctl.pgdir.tables[i].page[x].perms = tmp >> 32;
							this.ioctl.pgdir.tables[i].page[x].size = this.read(addr+1);
							this.ioctl.pgdir.tables[i].page[x].address = this.read(addr+2);
							
							addr += 3;
						}
						data += 1;
					}
					break;
				case 2:
					if(this.cpu.cores[this.cpu.currentCore].regs.flags[0] & CPU_REG_FLAG_PRIV_USER) return this.intr(CPU_INT["BADPERM"]);
					if(v > this.cpu.cores.length) return this.intr(CPU_INT["BADADDR"]);
					this.ioctl.selectedCore = v;
					break;
				case 3:
					if(this.cpu.cores[this.cpu.currentCore].regs.flags[0] & CPU_REG_FLAG_PRIV_USER) return this.intr(CPU_INT["BADPERM"]);
					if(this.ioctl.selectedCore > this.cpu.cores.length) return this.intr(CPU_INT["BADADDR"]);
					this.cpu.cores[this.ioctl.selectedCore].running = v;
					break;
				case 4:
					if(this.cpu.cores[this.cpu.currentCore].regs.flags[0] & CPU_REG_FLAG_PRIV_USER) return this.intr(CPU_INT["BADPERM"]);
					if(this.ioctl.selectedCore > this.cpu.cores.length) return this.intr(CPU_INT["BADADDR"]);
					this.cpu.cores[this.ioctl.selectedCore].regs.pc[0] = v;
					break;
					
			}
		});
		this.mmap(IO_RAM_BASE,IO_RAM_END,i => this.ioctl.ram[i],(i,v) => { this.ioctl.ram[i] = v; });
		if(this.ioctl.pgdir) delete this.ioctl.pgdir;
		
		/* Reset the mailbox */
		if(this.mailbox) this.mailbox.reset(this);
		
		/* Reset the RTC */
		if(this.rtc) this.rtc.reset(this);
		
		/* Reset the UART */
		if(this.uart) this.uart.reset(this);
		
		for(var i = 0;i < this.cpu.cores.length;i++) {
			this.cpu.cores[i] = setupCPUCore();
			
			/* Reset the registers */
			this.cpu.cores[i].regs = this.cpu.cores[i].iregs = setupRegisters();
			this.cpu.cores[i].irqs = [];
			this.cpu.cores[i].stack.fill(0);
			this.cpu.cores[i].ivt.fill(0);
			this.cpu.cores[i].intr[0] = 0;
			this.cpu.cores[i].running = false;
		}
		
		
		/* Reset the CPU */
		this.cpu.cores[this.cpu.currentCore].regs.pc[0] = IO_RAM_BASE;
		
		this.emit("reset");
	}
	
	intr(i) {
		if(i > this.cpu.cores[this.cpu.currentCore].ivt.length) throw new Error("SAVM_ERROR_INVAL_INT");
		if(i > 5 && this.cpu.cores[this.cpu.currentCore].regs.flags[0] & CPU_REG_FLAG_ENIRQ) return this.cpu.cores[this.cpu.currentCore].irqs.push(i);
		if(this.cpu.cores[this.cpu.currentCore].regs.flags[0] & CPU_REG_FLAG_INTR) {
			this.cpu.cores[this.cpu.currentCore].intr[0] = CPU_INT["FAULT"];
			return this.emit("cpu/interrupt",i);
		}
		if(i > 5 && !(this.cpu.cores[this.cpu.currentCore].regs.flags[0] & CPU_REG_FLAG_ENIRQ)) return;
		this.cpu.cores[this.cpu.currentCore].iregs = this.cpu.cores[this.cpu.currentCore].regs;
		this.cpu.cores[this.cpu.currentCore].regs.flags[0] |= CPU_REG_FLAG_INTR & CPU_REG_FLAG_PRIV_KERN;
		this.cpu.cores[this.cpu.currentCore].regs.flags[0] &= ~CPU_REG_FLAG_PRIV_USER;
		this.cpu.cores[this.cpu.currentCore].intr[0] = i;
		this.cpu.cores[this.cpu.currentCore].regs.pc[0] = this.cpu.cores[this.cpu.currentCore].ivt[i];
		
		this.emit("cpu/interrupt",i);
	}
	regread(i) {
		this.emit("cpu/registers/read",i);
		switch(i) {
			case 0: /* flags */ return this.cpu.cores[this.cpu.currentCore].regs.flags[0];
			case 1: /* tmp */ return this.cpu.cores[this.cpu.currentCore].regs.tmp[0];
			case 2: /* sp */ return this.cpu.cores[this.cpu.currentCore].regs.sp[0];
			case 3: /* ip */ return this.cpu.cores[this.cpu.currentCore].regs.ip[0];
			case 4: /* pc */ return this.cpu.cores[this.cpu.currentCore].regs.pc[0];
			case 5: /* cycle */ return this.cpu.cores[this.cpu.currentCore].regs.cycle[0];
			default:
				if(i >= 6 && i < 16) return this.cpu.cores[this.cpu.currentCore].regs.data[i-6];
				if(i >= 17 && i < 27) return this.cpu.cores[this.cpu.currentCore].regs.index[i-17];
				if(i >= 28 && i < 38) return this.cpu.cores[this.cpu.currentCore].regs.addr[i-28];
				if(i >= 39 && i < 49) return this.cpu.cores[this.cpu.currentCore].regs.ptr[i-39];
				throw new Error("SAVM_ERROR_INVAL_ADDR");
		}
	}
	regwrite(i,v) {
		switch(i) {
			case 0: /* flags */
				if(val & CPU_REG_FLAG_INTR && !(this.cpu.cores[this.cpu.currentCore].regs.flags[0] & CPU_REG_FLAG_INTR)) val &= ~CPU_REG_FLAG_INTR;
				if(val & CPU_REG_FLAG_PAGING && !(this.cpu.cores[this.cpu.currentCore].regs.flags[0] & CPU_REG_FLAG_PAGING)) val &= ~SAVM_CPU_REG_FLAG_PAGING;
				if(val & CPU_REG_FLAG_PRIV_KERN && !(this.cpu.cores[this.cpu.currentCore].regs.flags[0] & CPU_REG_FLAG_PRIV_KERN)) val &= ~SAVM_CPU_REG_FLAG_PRIV_KERN;
				if(val & CPU_REG_FLAG_PRIV_USER && !(this.cpu.cores[this.cpu.currentCore].regs.flags[0] & CPU_REG_FLAG_PRIV_USER)) val &= ~CPU_REG_FLAG_PRIV_USER;
				
				if(!(this.cpu.cores[this.cpu.currentCore].regs.flags[0] & SAVM_CPU_REG_FLAG_PAGING) && val & SAVM_CPU_REG_FLAG_PAGING) {
					if(this.ioctl.pgdir == null) return this.intr(CPU_INT["FAULT"]);
				}
				this.cpu.cores[this.cpu.currentCore].regs.flags[0] = v;
				break;
			case 1: /* tmp */
				this.cpu.cores[this.cpu.currentCore].regs.tmp[0] = v;
				break;
			case 2: /* sp */
				this.cpu.cores[this.cpu.currentCore].regs.sp[0] = v;
				break;
			case 3: /* ip */
				this.cpu.cores[this.cpu.currentCore].regs.ip[0] = v;
				break;
			case 4: /* pc */
				this.cpu.cores[this.cpu.currentCore].regs.pc[0] = v;
				break;
			case 5: /* cycle */
				this.cpu.cores[this.cpu.currentCore].regs.cycle[0] = v;
				break;
			default:
				if(i >= 6 && i < 16) {
					this.cpu.cores[this.cpu.currentCore].regs.data[i-6] = v;
					break;
				}
				if(i >= 17 && i < 27) {
					this.cpu.cores[this.cpu.currentCore].regs.index[i-17] = v;
					break;
				}
				if(i >= 28 && i < 38) {
					this.cpu.cores[this.cpu.currentCore].regs.addr[i-28] = v;
					break;
				}
				if(i >= 39 && i < 49) {
					this.cpu.cores[this.cpu.currentCore].regs.ptr[i-39] = v;
					break;
				}
				throw new Error("SAVM_ERROR_INVAL_ADDR");
		}
		this.emit("cpu/registers/write",i,v);
	}
	cycleCore(core) {
		if(core > this.cpu.cores.length) throw new Error("SAVM_ERROR_INVAL_ADDR");
		
		this.cpu.currentCore = core;
	
		if(this.cpu.cores[this.cpu.currentCore].regs.pc[0] == 0) this.cpu.cores[this.cpu.currentCore].regs.pc[0] = IO_RAM_BASE+(this.cpu.cores[this.cpu.currentCore].regs.cycle[0]*3);
		
		/* Fetches the instruction from memory */
		this.cpu.cores[this.cpu.currentCore].regs.ip[0] = this.read(this.cpu.cores[this.cpu.currentCore].regs.pc[0]);
		
		/* Decodes the instruction */
		var instr_opcode = BITS_EXTRACT(this.cpu.cores[this.cpu.currentCore].regs.ip[0],40,8);
		var instr_addrmode = BITS_EXTRACT(this.cpu.cores[this.cpu.currentCore].regs.ip[0],48,8);
		var instr_flags = BITS_EXTRACT(this.cpu.cores[this.cpu.currentCore].regs.ip[0],56,8);
		
		var addr = this.read(this.cpu.cores[this.cpu.currentCore].regs.pc[0]+1);
		var val = this.read(this.cpu.cores[this.cpu.currentCore].regs.pc[0]+2);
		
		this.cpu.cores[this.cpu.currentCore].regs.pc[0] += 3;
		
		/* Execute the instruction */
		try {
			switch(instr_opcode) {
				case 0: /* NOP */
					if(this.cpu.cores[this.cpu.currentCore].regs.flags & CPU_REG_FLAG_PRIV_KERN) this.stop();
					else this.intr(CPU_INT["BADPERM"]);
					break;
				case 1: /* ADD */
					switch(instr_addrmode) {
						case INSTR_ADDRMODE["REG"]:
							this.regwrite(addr,this.regread(addr)+this.regread(val));
							break;
						case INSTR_ADDRMODE["ADDR"]:
							this.write(addr,this.read(addr)+this.read(val));
							break;
						default: this.intr(CPU_INT["BADADDR"]);
							break;
					}
					break;
				case 2: /* SUB */
					switch(instr_addrmode) {
						case INSTR_ADDRMODE["REG"]:
							this.regwrite(addr,this.regread(addr)-this.regread(val));
							break;
						case INSTR_ADDRMODE["ADDR"]:
							this.write(addr,this.read(addr)-this.read(val));
							break;
						default: this.intr(CPU_INT["BADADDR"]);
							break;
					}
					break;
				case 3: /* MUL */
					switch(instr_addrmode) {
						case INSTR_ADDRMODE["REG"]:
							this.regwrite(addr,this.regread(addr)*this.regread(val));
							break;
						case INSTR_ADDRMODE["ADDR"]:
							this.write(addr,this.read(addr)*this.read(val));
							break;
						default: this.intr(CPU_INT["BADADDR"]);
							break;
					}
					break;
				case 4: /* DIV */
					switch(instr_addrmode) {
						case INSTR_ADDRMODE["REG"]:
							if(this.regread(val) == 0) {
								this.intr(CPU_INT["DIVBYZERO"]);
								break;
							}
							this.regwrite(addr,this.regread(addr)/this.regread(val));
							break;
						case INSTR_ADDRMODE["ADDR"]:
							if(this.read(val) == 0) {
								this.intr(CPU_INT["DIVBYZERO"]);
								break;
							}
							this.write(addr,this.read(addr)/this.read(val));
							break;
						default: this.intr(CPU_INT["BADADDR"]);
							break;
					}
					break;
				case 5: /* AND */
					switch(instr_addrmode) {
						case INSTR_ADDRMODE["REG"]:
							this.regwrite(addr,this.regread(addr) & this.regread(val));
							break;
						case INSTR_ADDRMODE["ADDR"]:
							this.write(addr,this.read(addr) & this.read(val));
							break;
						default: this.intr(CPU_INT["BADADDR"]);
							break;
					}
					break;
				case 6: /* OR */
					switch(instr_addrmode) {
						case INSTR_ADDRMODE["REG"]:
							this.regwrite(addr,this.regread(addr) | this.regread(val));
							break;
						case INSTR_ADDRMODE["ADDR"]:
							this.write(addr,this.read(addr) | this.read(val));
							break;
						default: this.intr(CPU_INT["BADADDR"]);
							break;
					}
					break;
				case 7: /* XOR */
					switch(instr_addrmode) {
						case INSTR_ADDRMODE["REG"]:
							this.regwrite(addr,this.regread(addr) ^ this.regread(val));
							break;
						case INSTR_ADDRMODE["ADDR"]:
							this.write(addr,this.read(addr) ^ this.read(val));
							break;
						default: this.intr(CPU_INT["BADADDR"]);
							break;
					}
					break;
				case 8: /* NOR */
					switch(instr_addrmode) {
						case INSTR_ADDRMODE["REG"]:
							this.regwrite(addr,~(this.regread(addr) | this.regread(val)));
							break;
						case INSTR_ADDRMODE["ADDR"]:
							this.write(addr,~(this.read(addr) | this.read(val)));
							break;
						default: this.intr(CPU_INT["BADADDR"]);
							break;
					}
					break;
				case 9: /* NAND */
					switch(instr_addrmode) {
						case INSTR_ADDRMODE["REG"]:
							this.regwrite(addr,~(this.regread(addr) & this.regread(val)));
							break;
						case INSTR_ADDRMODE["ADDR"]:
							this.write(addr,~(this.read(addr) & this.read(val)));
							break;
						default: this.intr(CPU_INT["BADADDR"]);
							break;
					}
					break;
				case 10: /* MOD */
					switch(instr_addrmode) {
						case INSTR_ADDRMODE["REG"]:
							this.regwrite(addr,this.regread(addr) % this.regread(val));
							break;
						case INSTR_ADDRMODE["ADDR"]:
							this.write(addr,this.read(addr) % this.read(val));
							break;
						default: this.intr(CPU_INT["BADADDR"]);
							break;
					}
					break;
				case 11: /* LSHIFT */
					switch(instr_addrmode) {
						case INSTR_ADDRMODE["REG"]:
							this.regwrite(addr,this.regread(addr) << this.regread(val));
							break;
						case INSTR_ADDRMODE["ADDR"]:
							this.write(addr,this.read(addr) << this.read(val));
							break;
						default: this.intr(CPU_INT["BADADDR"]);
							break;
					}
					break;
				case 12: /* RSHIFT */
					switch(instr_addrmode) {
						case INSTR_ADDRMODE["REG"]:
							this.regwrite(addr,this.regread(addr) >> this.regread(val));
							break;
						case INSTR_ADDRMODE["ADDR"]:
							this.write(addr,this.read(addr) >> this.read(val));
							break;
						default: this.intr(CPU_INT["BADADDR"]);
							break;
					}
					break;
				case 13: /* CMP */
					switch(instr_addrmode) {
						case INSTR_ADDRMODE["REG"]:
							this.cpu.cores[this.cpu.currentCore].regs.tmp[0] = this.regread(addr) == this.regread(val);
							break;
						case INSTR_ADDRMODE["ADDR"]:
							this.cpu.cores[this.cpu.currentCore].regs.tmp[0] = this.read(addr) == this.read(val);
							break;
						default: this.intr(CPU_INT["BADADDR"]);
							break;
					}
					break;
				case 14: /* GRTN */
					switch(instr_addrmode) {
						case INSTR_ADDRMODE["REG"]:
							this.cpu.cores[this.cpu.currentCore].regs.tmp[0] = this.regread(addr) > this.regread(val);
							break;
						case INSTR_ADDRMODE["ADDR"]:
							this.cpu.cores[this.cpu.currentCore].regs.tmp[0] = this.read(addr) > this.read(val);
							break;
						default: this.intr(CPU_INT["BADADDR"]);
							break;
					}
					break;
				case 15: /* LSTN */
					switch(instr_addrmode) {
						case INSTR_ADDRMODE["REG"]:
							this.cpu.cores[this.cpu.currentCore].regs.tmp[0] = this.regread(addr) < this.regread(val);
							break;
						case INSTR_ADDRMODE["ADDR"]:
							this.cpu.cores[this.cpu.currentCore].regs.tmp[0] = this.read(addr) < this.read(val);
							break;
						default: this.intr(CPU_INT["BADADDR"]);
							break;
					}
					break;
				case 16: /* JIT */
					switch(instr_addrmode) {
						case INSTR_ADDRMODE["REG"]:
							if(this.cpu.cores[this.cpu.currentCore].regs.tmp[0]) this.cpu.cores[this.cpu.currentCore].regs.pc[0] = this.regread(addr);
							break;
						case INSTR_ADDRMODE["ADDR"]:
							if(this.cpu.cores[this.cpu.currentCore].regs.tmp[0]) this.cpu.cores[this.cpu.currentCore].regs.pc[0] = this.read(addr);
							break;
						default: this.intr(CPU_INT["BADADDR"]);
							break;
					}
					break;
				/* Memory Instructions */
				case 17: /* JMP */
					switch(instr_addrmode) {
						case INSTR_ADDRMODE["REG"]:
							this.cpu.cores[this.cpu.currentCore].regs.pc[0] = this.regread(addr);
							break;
						case INSTR_ADDRMODE["ADDR"]:
							this.cpu.cores[this.cpu.currentCore].regs.pc[0] = this.read(addr);
							break;
						default: this.intr(CPU_INT["BADADDR"]);
							break;
					}
					break;
				case 18: /* CALL */
					if(this.cpu.cores[this.cpu.currentCore].regs.sp[0] < this.cpu.cores[this.cpu.currentCore].stack.length) {
						this.cpu.cores[this.cpu.currentCore].stack[this.cpu.cores[this.cpu.currentCore].regs.sp[0]++] = this.cpu.cores[this.cpu.currentCore].regs.pc[0];
						switch(instr_addrmode) {
							case INSTR_ADDRMODE["REG"]:
								this.cpu.cores[this.cpu.currentCore].regs.pc[0] = this.regread(addr);
								break;
							case INSTR_ADDRMODE["ADDR"]:
								this.cpu.cores[this.cpu.currentCore].regs.pc[0] = this.read(addr);
								break;
							default: this.intr(CPU_INT["BADADDR"]);
								break;
						}
					} else this.intr(CPU_INT["STACK_OVERFLOW"]);
					break;
				case 19: /* RET */
					if(this.cpu.cores[this.cpu.currentCore].regs.sp[0] < this.cpu.cores[this.cpu.currentCore].stack.length) this.cpu.cores[this.cpu.currentCore].regs.pc[0] = this.cpu.cores[this.cpu.currentCore].stack[this.cpu.cores[this.cpu.currentCore].regs.sp[0]--];
					else this.intr(CPU_INT["STACK_OVERFLOW"]);
					break;
				case 20: /* PUSH */
					if(this.cpu.cores[this.cpu.currentCore].regs.sp[0] < this.cpu.cores[this.cpu.currentCore].stack.length) {
						switch(instr_addrmode) {
							case INSTR_ADDRMODE["REG"]:
								this.cpu.cores[this.cpu.currentCore].stack[this.cpu.cores[this.cpu.currentCore].regs.sp[0]++] = this.regread(addr);
								break;
							case INSTR_ADDRMODE["ADDR"]:
								this.cpu.cores[this.cpu.currentCore].stack[this.cpu.cores[this.cpu.currentCore].regs.sp[0]++] = this.read(addr);
								break;
							default: this.intr(CPU_INT["BADADDR"]);
								break;
						}
					} else this.intr(CPU_INT["STACK_OVERFLOW"]);
					break;
				case 21: /* POP */
					if(this.cpu.cores[this.cpu.currentCore].regs.sp[0] < this.cpu.cores[this.cpu.currentCore].stack.length) {
						switch(instr_addrmode) {
							case INSTR_ADDRMODE["REG"]:
								this.regwrite(addr,this.cpu.cores[this.cpu.currentCore].stack[this.cpu.cores[this.cpu.currentCore].regs.sp[0]--]);
								break;
							case INSTR_ADDRMODE["ADDR"]:
								this.write(addr,this.cpu.cores[this.cpu.currentCore].stack[this.cpu.cores[this.cpu.currentCore].regs.sp[0]--]);
								break;
							default: this.intr(CPU_INT["BADADDR"]);
								break;
						}
					} else this.intr(CPU_INT["STACK_OVERFLOW"]);
					break;
				case 22: /* MOVR */
					switch(instr_addrmode) {
						case INSTR_ADDRMODE["REG"]:
							this.regwrite(addr,this.regread(val));
							break;
						case INSTR_ADDRMODE["ADDR"]:
							this.regwrite(addr,this.read(val));
							break;
						case INSTR_ADDRMODE["RAW"]:
							this.regwrite(addr,val);
							break;
						default: this.intr(CPU_INT["BADADDR"]);
							break;
					}
					break;
				case 23: /* MOVM */
					switch(instr_addrmode) {
						case INSTR_ADDRMODE["REG"]:
							this.write(addr,this.regread(val));
							break;
						case INSTR_ADDRMODE["ADDR"]:
							this.write(addr,this.read(val));
							break;
						case INSTR_ADDRMODE["RAW"]:
							this.write(addr,val);
							break;
						default: this.intr(CPU_INT["BADADDR"]);
							break;
					}
					break;
				/* Interupt Instructions */
				case 24: /* INT */
					if(this.cpu.cores[this.cpu.currentCore].regs.flags & CPU_REG_FLAG_PRIV_KERN) {
						var a;
						switch(instr_addrmode) {
							case INSTR_ADDRMODE["REG"]:
								a = this.regread(val);
								break;
							case INSTR_ADDRMODE["ADDR"]:
								a = this.read(val);
								break;
							case INSTR_ADDRMODE["RAW"]:
								a = val;
								break;
							default: this.intr(CPU_INT["BADADDR"]);
								break;
						}
						try {
							this.intr(a);
						} catch(ex) {
							this.intr(CPU_INT["BADADDR"]);
						}
					} else this.intr(CPU_INT["BADPERM"]);
					break;
				case 25: /* IRET */
					if(this.cpu.cores[this.cpu.currentCore].regs.flags & CPU_REG_FLAG_PRIV_KERN) {
						if(this.cpu.cores[this.cpu.currentCore].regs.flags[0] & CPU_REG_FLAG_INTR) this.cpu.cores[this.cpu.currentCore].regs = this.cpu.cores[this.cpu.currentCore].iregs;
						else this.intr(CPU_INT["FAULT"]);
					} else this.intr(CPU_INT["BADPERM"]);
					break;
				case 26: /* LDITBL */
					if(this.cpu.cores[this.cpu.currentCore].regs.flags & CPU_REG_FLAG_PRIV_KERN) {
						switch(instr_addrmode) {
							case INSTR_ADDRMODE["REG"]:
								addr = this.regread(addr);
								break;
							case INSTR_ADDRMODE["ADDR"]:
								addr = this.read(addr);
								break;
							case INSTR_ADDRMODE["RAW"]:
								break;
							default: this.intr(CPU_INT["BADADDR"]);
								break;
						}
						for(var i = 0;i < this.cpu.cores[this.cpu.currentCore].ivt.length;i++) this.cpu.cores[this.cpu.currentCore].ivt[i] = this.read(addr+i);
					} else this.intr(CPU_INT["BADPERM"]);
					break;
				case 27: /* RST */
					if(this.cpu.cores[this.cpu.currentCore].regs.flags & CPU_REG_FLAG_PRIV_KERN) {
						this.stop();
						this.reset();
						this.start();
						this.cpu.cores[this.cpu.currentCore].regs.cycle[0] = -1;
					} else this.intr(CPU_INT["BADPERM"]);
					break;
				default:
					this.intr(CPU_INT["BADINSTR"]);
			}
		} catch(ex) {
			if(ex.message == "SAVM_ERROR_NOTMAPPED") this.intr(CPU_INT["BADADDR"]);
			else throw ex;
		}
		
		if(this.rtc) this.rtc.cycle(this);
		if(this.mailbox) this.mailbox.cycle(this);
		if(this.uart) this.uart.cycle(this);
		
		if(this.cpu.cores[this.cpu.currentCore].regs.flags[0] & CPU_REG_FLAG_INTR && this.cpu.cores[this.cpu.currentCore].regs.flags[0] & CPU_REG_FLAG_ENIRQ && this.cpu.cores[this.cpu.currentCore].irqs.length > 0) this.intr(this.cpu.cores[this.cpu.currentCore].irqs.shift());
		
		this.emit("cpu/cycleCore",core);
		
		this.cpu.cores[this.cpu.currentCore].regs.cycle[0]++;
	}
	cycle() {
		for(var i = 0;i < this.cpu.cores.length;i++) {
			if(this.cpu.cores[i].running) this.cycleCore(i);
		}
		this.emit("cpu/cycle");
	}
	
	_buff2uint64(buff) {
		var u64 = new Float64Array(buff.length/8);
		var index = 0;
		for(var i = 0;i < buff.length;i += 8) {
			var b = buff.slice(i,i+8);
			var arr = new Int32Array(new Uint8Array(buff.slice(i,i+8)).buffer);
			var v = new jints.UInt64.join(arr[1],arr[0]);
			v = parseInt(v.toString());
			u64[index++] = v;
		}
		return u64;
	}
	_uint642buff(u64) {
		var buff = Buffer.alloc(u64.length*8);
		var index = 0;
		for(var i = 0;i < u64.length;i++) {
			var arr = new jints.UInt64(u64[i]).toArray();
			for(var x = 0;x < 8;x++) buff[index++] = arr[x];
		}
		return buff;
	}
	
	start() {
		this.cpu.cores[0].running = true;
		this.emit("cpu/start");
		this.interval = setInterval(() => {
			this.cycle();
			if(!this.cpu.cores[0].running) this.stop();
		},this.cpu.clockspeed);
	}
	stop() {
		if(typeof(this.interval) == "number") clearInterval(this.interval);
		this.cpu.cores[0].running = false;
		this.emit("cpu/stop");
	}
	loadFirmware(path) {
		var buff = this._buff2uint64(fs.readFileSync(path));
		for(var i = 0;i < buff.length;i++) this.ioctl.ram[i] = buff[i];
	}
	loadFile(addr,path) {
		var buff = this._buff2uint64(fs.readFileSync(path));
		for(var i = 0;i < buff.length;i++) this.write(addr,buff[i]);
	}
	dumpFile(addr,size,path) {
		var buff = new Float64Array(size);
		for(var i = 0;i < buff.length;i++) buff[i] = this.read(i);
		fs.writeFileSync(path,this._uint642buff(buff));
	}
	mmap(addr,end,read = () => 0,write = () => {}) {
		for(var entry of this.ioctl.mmap) {
			if(entry.addr == addr && entry.end == end) throw new Error("SAVM_ERROR_MAPPED");
		}
		var i = this.ioctl.mmap.push({
			addr: addr,
			end: end,
			read: read,
			write: write
		})-1;
		this.emit("ioctl/mmap",addr,end,i);
	}
	isMapped(addr) {
		for(var entry of this.ioctl.mmap) {
			if(this.isMappedAt(addr,entry)) return true;
		}
		return false;
	}
	isMappedAt(addr,entry) {
		return entry.addr <= addr && entry.end > addr;
	}
	read(addr) {
		this.emit("ioctl/read",addr);
		if(this.cpu.cores[this.cpu.currentCore].regs.flags[0] & CPU_REG_FLAG_PAGING) {
			for(var table of this.ioctl.pgdir.tables) {
				for(var pg of table.entries) {
					if(pg.address <= addr && pg.address+pg.size > addr) {
						if(this.cpu.cores[this.cpu.currentCore].regs.flags[0] & CPU_REG_FLAG_PRIV_KERN && !(pg.perms & CPU_PAGING_PERM_KERN)) return this.intr(CPU_INT["BADPERM"]);
						if(this.cpu.cores[this.cpu.currentCore].regs.flags[0] & CPU_REG_FLAG_PRIV_USER && !(pg.perms & CPU_PAGING_PERM_USER)) return this.intr(CPU_INT["BADPERM"]);
					}
				}
			}
		}
		for(var entry of this.ioctl.mmap) {
			if(this.isMappedAt(addr,entry)) return entry.read(addr-entry.addr);
		}
		throw new Error("SAVM_ERROR_NOTMAPPED: 0x"+addr.toString(16));
	}
	write(addr,v) {
		if(this.cpu.cores[this.cpu.currentCore].regs.flags[0] & CPU_REG_FLAG_PAGING) {
			for(var table of this.ioctl.pgdir.tables) {
				for(var pg of table.entries) {
					if(pg.address <= addr && pg.address+pg.size > addr) {
						if(this.cpu.cores[this.cpu.currentCore].regs.flags[0] & CPU_REG_FLAG_PRIV_KERN && !(pg.perms & CPU_PAGING_PERM_KERN)) return this.intr(CPU_INT["BADPERM"]);
						if(this.cpu.cores[this.cpu.currentCore].regs.flags[0] & CPU_REG_FLAG_PRIV_USER && !(pg.perms & CPU_PAGING_PERM_USER)) return this.intr(CPU_INT["BADPERM"]);
					}
				}
			}
		}
		for(var entry of this.ioctl.mmap) {
			if(this.isMappedAt(addr,entry)) {
				entry.write(addr-entry.addr,v);
				return this.emit("ioctl/write",addr,v);
			}
		}
		throw new Error("SAVM_ERROR_NOTMAPPED");
	}
}

module.exports = VirtualMachine;
