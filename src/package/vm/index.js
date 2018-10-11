const EventEmitter = require("events");
const fs = require("fs");
const jints = require("jints");
const Mailbox = require("./hardware/mailbox");
const RTC = require("./hardware/rtc");
const UART = require("./hardware/uart");

const CPU_REG_FLAG_INTR = (1 << 0);
const CPU_REG_FLAG_ENIRQ = (1 << 1);

const CPU_INT = {
	"STACK_OVERFLOW": 0,
	"FAULT": 1,
	"BADADDR": 2,
	"DIVBYZERO": 3,
	"BADINSTR": 4,
	"TIMER": 5,
	"MAILBOX": 6
};

const IO_RAM_BASE = 0xA0000000;
const IO_RAM_SIZE = 0x40000000;
const IO_RAM_END = IO_RAM_BASE+IO_RAM_SIZE;

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

class VirtualMachine extends EventEmitter {
	constructor(opts = {}) {
		super();
		this.cpu = {
			regs: setupRegisters(),
			iregs: setupRegisters(),
			
			stack: new Float64Array(20),
			ivt: new Float64Array(6),
			intr: new Float64Array(1),
			running: false,
			clockspeed: 1,
			irqs: []
		};
		this.ioctl = {
			ram: new Float64Array(IO_RAM_SIZE),
			mmap: []
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
		this.ioctl.ram = new Float64Array(IO_RAM_SIZE);
		
		/* (Re)initialize the memory map in the IO Controller */
		if(this.ioctl.mmap) delete this.ioctl.mmap;
		this.ioctl.mmap = [];
		this.mmap(IO_RAM_BASE,IO_RAM_END,i => this.ioctl.ram[i],(i,v) => { this.ioctl.ram[i] = v; });
		
		/* Reset the mailbox */
		if(this.mailbox) this.mailbox.reset(this);
		
		/* Reset the RTC */
		if(this.rtc) this.rtc.reset(this);
		
		/* Reset the UART */
		if(this.uart) this.uart.reset(this);
		
		/* Reset the registers */
		this.cpu.regs = this.cpu.iregs = setupRegisters();
		
		/* Reset the CPU */
		this.cpu.irqs = [];
		this.cpu.stack.fill(0);
		this.cpu.ivt.fill(0);
		this.cpu.intr[0] = 0;
		this.cpu.running = false;
		
		this.emit("reset");
	}
	
	intr(i) {
		if(i > this.cpu.ivt.length) throw new Error("SAVM_ERROR_INVAL_INT");
		if(i > 4 && this.cpu.regs.flags[0] & CPU_REG_FLAG_ENIRQ) return this.cpu.irqs.push(i);
		if(this.cpu.regs.flags[0] & CPU_REG_FLAG_INTR) {
			this.cpu.intr[0] = CPU_INT["FAULT"];
			return this.emit("cpu/interrupt",i);
		}
		if(i > 4 && !(this.cpu.regs.flags[0] & CPU_REG_FLAG_ENIRQ)) return;
		this.cpu.iregs = this.cpu.regs;
		this.cpu.regs.flags[0] |= CPU_REG_FLAG_INTR;
		this.cpu.intr[0] = i;
		this.cpu.regs.pc[0] = this.cpu.ivt[i];
		
		this.emit("cpu/interrupt",i);
	}
	regread(i) {
		this.emit("cpu/registers/read",i);
		switch(i) {
			case 0: /* flags */ return this.cpu.regs.flags[0];
			case 1: /* tmp */ return this.cpu.regs.tmp[0];
			case 2: /* sp */ return this.cpu.regs.sp[0];
			case 3: /* ip */ return this.cpu.regs.ip[0];
			case 4: /* pc */ return this.cpu.regs.pc[0];
			case 5: /* cycle */ return this.cpu.regs.cycle[0];
			default:
				if(i >= 6 && i < 16) return this.cpu.regs.data[i-6];
				if(i >= 17 && i < 27) return this.cpu.regs.index[i-17];
				if(i >= 28 && i < 38) return this.cpu.regs.addr[i-28];
				if(i >= 39 && i < 49) return this.cpu.regs.ptr[i-39];
				throw new Error("SAVM_ERROR_INVAL_ADDR");
		}
	}
	regwrite(i,v) {
		switch(i) {
			case 0: /* flags */
				if(val & CPU_REG_FLAG_INTR && !(this.cpu.regs.flags[0] & CPU_REG_FLAG_INTR)) val &= ~CPU_REG_FLAG_INTR;
				this.cpu.regs.flags[0] = v;
				break;
			case 1: /* tmp */
				this.cpu.regs.tmp[0] = v;
				break;
			case 2: /* sp */
				this.cpu.regs.sp[0] = v;
				break;
			case 3: /* ip */
				this.cpu.regs.ip[0] = v;
				break;
			case 4: /* pc */
				this.cpu.regs.pc[0] = v;
				break;
			case 5: /* cycle */
				this.cpu.regs.cycle[0] = v;
				break;
			default:
				if(i >= 6 && i < 16) {
					this.cpu.regs.data[i-6] = v;
					break;
				}
				if(i >= 17 && i < 27) {
					this.cpu.regs.index[i-17] = v;
					break;
				}
				if(i >= 28 && i < 38) {
					this.cpu.regs.addr[i-28] = v;
					break;
				}
				if(i >= 39 && i < 49) {
					this.cpu.regs.ptr[i-39] = v;
					break;
				}
				throw new Error("SAVM_ERROR_INVAL_ADDR");
		}
		this.emit("cpu/registers/write",i,v);
	}
	cycle() {
		if(this.cpu.regs.pc[0] == 0) this.cpu.regs.pc[0] = IO_RAM_BASE+(this.cpu.regs.cycle[0]*3);
		
		/* Fetches the instruction from memory */
		this.cpu.regs.ip[0] = this.read(this.cpu.regs.pc[0]);
		
		/* Decodes the instruction */
		var addr = this.read(this.cpu.regs.pc[0]+1);
		var val = this.read(this.cpu.regs.pc[0]+2);
		
		this.cpu.regs.pc[0] += 3;
		
		/* Execute the instruction */
		try {
			switch(this.cpu.regs.ip[0]) {
				case 0: /* NOP */
					this.stop();
					break;
				case 1: /* ADDR */
					this.regwrite(addr,this.regread(addr)+this.regread(val));
					break;
				case 2: /* ADDM */
					this.write(addr,this.read(addr)+this.read(val));
					break;
				case 3: /* SUBR */
					this.regwrite(addr,this.regread(addr)-this.regread(val));
					break;
				case 4: /* SUBM */
					this.write(addr,this.read(addr)-this.read(val));
					break;
				case 5: /* MULR */
					this.regwrite(addr,this.regread(addr)*this.regread(val));
					break;
				case 6: /* MULM */
					this.write(addr,this.read(addr)*this.read(val));
					break;
				case 7: /* DIVR */
					if(this.regread(val) == 0) {
						this.intr(CPU_INT["DIVBYZERO"]);
						break;
					}
					this.regwrite(addr,this.regread(addr)/this.regread(val));
					break;
				case 8: /* DIVM */
					if(this.read(val) == 0) {
						this.intr(CPU_INT["DIVBYZERO"]);
						break;
					}
					this.write(addr,this.read(addr)/this.read(val));
					break;
				case 9: /* ANDR */
					this.regwrite(addr,this.regread(addr) & this.regread(val));
					break;
				case 10: /* ANDM */
					this.write(addr,this.read(addr) & this.read(val));
					break;
				case 11: /* ORR */
					this.regwrite(addr,this.regread(addr) | this.regread(val));
					break;
				case 12: /* ORM */
					this.write(addr,this.read(addr) | this.read(val));
					break;
				case 13: /* XORR */
					this.regwrite(addr,this.regread(addr) ^ this.regread(val));
					break;
				case 14: /* XORM */
					this.write(addr,this.read(addr) ^ this.read(val));
					break;
				case 15: /* NORR */
					this.regwrite(addr,~(this.regread(addr) | this.regread(val)));
					break;
				case 16: /* NORM */
					this.write(addr,~(this.read(addr) | this.read(val)));
					break;
				case 17: /* NANDR */
					this.regwrite(addr,~(this.regread(addr) & this.regread(val)));
					break;
				case 18: /* NANDM */
					this.write(addr,~(this.read(addr) & this.read(val)));
					break;
				case 19: /* LSHIFTR */
					this.regwrite(addr,this.regread(addr) << this.regread(val));
					break;
				case 20: /* LSHIFTM */
					this.write(addr,this.read(addr) << this.read(val));
					break;
				case 21: /* RSHIFTR */
					this.regwrite(addr,this.regread(addr) >> this.regread(val));
					break;
				case 22: /* RSHIFTM */
					this.write(addr,this.read(addr) >> this.read(val));
					break;
				case 23: /* CMPR */
					this.cpu.regs.tmp[0] = this.regread(addr) == this.regread(val);
					break;
				case 24: /* CMPM */
					this.cpu.regs.tmp[0] = this.read(addr) == this.read(val);
					break;
				case 25: /* JITR */
					if(this.cpu.regs.tmp[0]) this.cpu.regs.pc[0] = this.regread(addr);
					break;
				case 26: /* JITM */
					if(this.cpu.regs.tmp[0]) this.cpu.regs.pc[0] = this.read(addr);
					break;
				case 27: /* JIT */
					if(this.cpu.regs.tmp[0]) this.cpu.regs.pc[0] = addr;
					break;
				case 28: /* JMPR */
					this.cpu.regs.pc[0] = this.regread(addr);
					break;
				case 29: /* JMPM */
					this.cpu.regs.pc[0] = this.read(addr);
					break;
				case 30: /* JMP */
					this.cpu.regs.pc[0] = addr;
					break;
				case 31: /* CALLR */
					if(this.cpu.regs.sp[0] < this.cpu.stack.length) {
						this.cpu.stack[this.cpu.regs.sp[0]++] = this.cpu.regs.pc[0];
						this.cpu.regs.pc[0] = this.regread(addr);
					} else this.intr(CPU_INT["STACK_OVERFLOW"]);
					break;
				case 32: /* CALLM */
					if(this.cpu.regs.sp[0] < this.cpu.stack.length) {
						this.cpu.stack[this.cpu.regs.sp[0]++] = this.cpu.regs.pc[0];
						this.cpu.regs.pc[0] = this.read(addr);
					} else this.intr(CPU_INT["STACK_OVERFLOW"]);
					break;
				case 33: /* CALL */
					if(this.cpu.regs.sp[0] < this.cpu.stack.length) {
						this.cpu.stack[this.cpu.regs.sp[0]++] = this.cpu.regs.pc[0];
						this.cpu.regs.pc[0] = addr;
					} else this.intr(CPU_INT["STACK_OVERFLOW"]);
					break;
				case 34: /* RET */
					if(this.cpu.regs.sp[0] < this.cpu.stack.length) this.cpu.regs.pc[0] = this.cpu.stack[this.cpu.regs.sp[0]--];
					else this.intr(CPU_INT["STACK_OVERFLOW"]);
					break;
				case 35: /* PUSHR */
					if(this.cpu.regs.sp[0] < this.cpu.stack.length) this.cpu.stack[this.cpu.regs.sp[0]++] = this.regread(addr);
					else this.intr(CPU_INT["STACK_OVERFLOW"]);
					break;
				case 36: /* PUSHM */
					if(this.cpu.regs.sp[0] < this.cpu.stack.length) this.cpu.stack[this.cpu.regs.sp[0]++] = this.read(addr);
					else this.intr(CPU_INT["STACK_OVERFLOW"]);
					break;
				case 37: /* POPR */
					if(this.cpu.regs.sp[0] < this.cpu.stack.length) this.regwrite(addr,this.cpu.stack[this.cpu.regs.sp[0]--]);
					else this.intr(CPU_INT["STACK_OVERFLOW"]);
					break;
				case 38: /* POPM */
					if(this.cpu.regs.sp[0] < this.cpu.stack.length) this.write(addr,this.cpu.stack[this.cpu.regs.sp[0]--]);
					else this.intr(CPU_INT["STACK_OVERFLOW"]);
					break;
				case 39: /* MOVRR */
					this.regwrite(addr,this.regread(val));
					break;
				case 40: /* MOVRM */
					this.write(addr,this.regread(val));
					break;
				case 41: /* MOVMR */
					this.regwrite(addr,this.read(val));
					break;
				case 42: /* MOVMM */
					this.write(addr,this.read(val));
					break;
				case 43: /* STOR */
					this.regwrite(addr,val);
					break;
				case 44: /* STOM */
					this.write(addr,val);
					break;
				case 45: /* INTR */
					var a = this.regread(addr);
					try {
						this.intr(a);
					} catch(ex) {
						this.intr(CPU_INT["BADADDR"]);
					}
					break;
				case 46: /* INTM */
					var a = this.read(addr);
					try {
						this.intr(a);
					} catch(ex) {
						this.intr(CPU_INT["BADADDR"]);
					}
					break;
				case 47: /* INT */
					try {
						this.intr(addr);
					} catch(ex) {
						this.intr(CPU_INT["BADADDR"]);
					}
					break;
				case 48: /* IRET */
					if(this.cpu.regs.flags[0] & CPU_REG_FLAG_INTR) this.cpu.regs = this.cpu.iregs;
					else this.intr(CPU_INT["FAULT"]);
					break;
				case 49: /* LDITBLR */
					addr = this.regread(addr);
					for(var i = 0;i < this.cpu.ivt.length;i++) this.cpu.ivt[i] = this.read(addr+i);
					break;
				case 50: /* LDITBLM */
					addr = this.read(addr);
					for(var i = 0;i < this.cpu.ivt.length;i++) this.cpu.ivt[i] = this.read(addr+i);
					break;
				case 51: /* HLT */
					this.stop();
					break;
				case 52: /* RST */
					this.stop();
					this.reset();
					this.start();
					this.cpu.regs.cycle[0] = -1;
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
		
		if(this.cpu.regs.flags[0] & CPU_REG_FLAG_INTR && this.cpu.regs.flags[0] & CPU_REG_FLAG_ENIRQ && this.cpu.irqs.length > 0) this.intr(this.cpu.irqs.shift());
		
		this.emit("cpu/cycle");
		
		this.cpu.regs.cycle[0]++;
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
		this.cpu.running = true;
		this.emit("cpu/start");
		this.interval = setInterval(() => {
			this.cycle();
			if(!this.cpu.running) this.stop();
		},this.cpu.clockspeed);
	}
	stop() {
		if(typeof(this.interval) == "number") clearInterval(this.interval);
		this.cpu.running = false;
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
	read(addr) {
		this.emit("ioctl/read",addr);
		for(var entry of this.ioctl.mmap) {
			if(entry.addr <= addr && entry.end > addr) return entry.read(addr-entry.addr);
		}
		throw new Error("SAVM_ERROR_NOTMAPPED");
	}
	write(addr,v) {
		for(var entry of this.ioctl.mmap) {
			if(entry.addr <= addr && entry.end > addr) {
				entry.write(addr-entry.addr,v);
				return this.emit("ioctl/write",addr,v);
			}
		}
		throw new Error("SAVM_ERROR_NOTMAPPED");
	}
}

module.exports = VirtualMachine;
