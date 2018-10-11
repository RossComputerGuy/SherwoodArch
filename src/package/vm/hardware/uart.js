const EventEmitter = require("events");

const UART_BASE = 0x10000012;
const UART_SIZE = 0x00000003;
const UART_END = UART_BASE+UART_SIZE;

class UART extends EventEmitter {
	constructor() {
		super();
		
		this.input = [];
		
		this.on("input",(d) => {
			this.input.push(d);
		});
	}
	destroy() {}
	reset(vm) {
		vm.mmap(UART_BASE,UART_END,i => {
			switch(i) {
				case 0: return this.input.length > 0;
				case 1: return this.input.shift() || 0;
				case 2: return 1;
			}
			return 0;
		},(i,v) => {
			switch(i) {
				case 1: this.emit("output",Buffer.from([v]).toString());
			}
		});
	}
	cycle(vm) {
	}
}
module.exports = UART;
