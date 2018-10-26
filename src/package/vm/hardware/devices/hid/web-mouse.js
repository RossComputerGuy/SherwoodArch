const EventEmitter = require("events");

const FLAGS = {
	"ENABLE": (1 << 0)
};

class MouseHIDDevice extends EventEmitter {
	constructor(vm) {
		super();
		this.hdr = {
			vendorID: 0xA900,
			deviceID: 0x0004,
			rev: 0,
			type: 7,
			classCode: 0,
			subclass: 0
		};
		
		this.name = "Basic Mouse Device";
		
		this.flags = 0;
		this.input = [];
		
		this.on("input",(d) => {
			if(this.flags & FLAGS["ENABLE"]) {
				this.input.push(d);
				vm.mailbox.intr(this);
			}
		});
	}
	create(dev,vm) {
		this.input = [];
		window.addEventListener("mousemove",this.eventMouseMove,false);
	}
	destroy(dev) {
		window.removeEventListener("mousemove",this.eventMouseMove,false);
	}
	eventMouseMove(event) {
		if(!(this.flags & FLAGS["ENABLE"])) return;
		this.emit("input",event.button);
	}
	cycle(dev,vm) {
	}
	read(dev,i,vm) {
		if(!(this.flags & FLAGS["ENABLE"])) return 0;
		switch(i) {
			case 0: return this.flags;
			case 1: return window.event.clientX;
			case 2: return window.event.clientY;
			case 3: return this.input.length > 0;
			case 4: return this.input.shift() || 0;
		}
		return 0;
	}
	write(dev,i,v,vm) {
		switch(i) {
			case 0:
				this.flags = v;
				if(v & FLAGS["ENABLE"]) this.input = [];
				break;
		}
	}
}
module.exports = MouseHIDDevice;
