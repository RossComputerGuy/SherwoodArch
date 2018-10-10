const assert = require("assert").ok;

const MAILBOX_BASE = 0x10000000;
const MAILBOX_SIZE = 0x00000009;
const MAILBOX_END = MAILBOX_BASE+MAILBOX_SIZE;

class Mailbox {
	constructor() {
		this.devices = [];
		this.devIndex = 0;
		this.dataIndex = 0;
	}
	destroy() {
		for(var dev of this.devices) {
			if(dev.destroy) dev.destroy(dev,this);
		}
	}
	isValidDeviceIndex(i) {
		return i >= 0 && this.devices.length > i;
	}
	reset(vm) {
		vm.mmap(MAILBOX_BASE,MAILBOX_END,i => {
			switch(i) {
				case 0: return this.devices.length;
				case 1: return this.devIndex;
				case 2: return this.dataIndex;
				case 3:
					if(this.isValidDeviceIndex(this.devIndex)) {
						if(this.devices[this.devIndex].read) return this.devices[this.devIndex].read(this.devices[this.devIndex],this.dataIndex);
					} else return 0xFFFF;
				case 4:
					if(this.isValidDeviceIndex(this.devIndex)) return this.devices[this.devIndex].hdr.vendorID;
					else return 0xFFFF;
				case 5:
					if(this.isValidDeviceIndex(this.devIndex)) return this.devices[this.devIndex].hdr.deviceID;
					else return 0xFFFF;
				case 6:
					if(this.isValidDeviceIndex(this.devIndex)) return this.devices[this.devIndex].hdr.rev;
					else return 0xFFFF;
				case 7:
					if(this.isValidDeviceIndex(this.devIndex)) return this.devices[this.devIndex].hdr.type;
					else return 0xFFFF;
				case 8:
					if(this.isValidDeviceIndex(this.devIndex)) return this.devices[this.devIndex].hdr.classCode;
					else return 0xFFFF;
				case 9:
					if(this.isValidDeviceIndex(this.devIndex)) return this.devices[this.devIndex].hdr.subclass;
					else return 0xFFFF;
			}
			return 0;
		},(i,v) => {
			switch(i) {
				case 1:
					this.devIndex = v;
					break;
				case 2:
					this.dataIndex = v;
					break;
				case 3:
					if(this.isValidDeviceIndex(this.devIndex)) {
						if(this.devices[this.devIndex].write) return this.devices[this.devIndex].write(this.devices[this.devIndex],this.dataIndex,v);
					} else return 0xFFFF;
			}
		});
		for(var dev of this.devices) {
			if(dev.reset) dev.reset(dev,vm);
		}
	}
	cycle(vm) {
		for(var dev of this.devices) {
			if(dev.cycle) dev.cycle(dev,vm);
		}
	}
	addDevice(dev) {
		assert(typeof(dev) == "object","dev is not an object");
		assert(typeof(dev.hdr) == "object","dev.hdr is not an object");
		assert(typeof(dev.hdr.vendorID) == "number","dev.hdr.vendorID is not a number");
		assert(typeof(dev.hdr.deviceID) == "number","dev.hdr.deviceID is not a number");
		assert(typeof(dev.hdr.rev) == "number","dev.hdr.rev is not a number");
		assert(typeof(dev.hdr.type) == "number","dev.hdr.type is not a number");
		assert(typeof(dev.hdr.classCode) == "number","dev.hdr.classCode is not a number");
		assert(typeof(dev.hdr.subclass) == "number","dev.hdr.subclass is not a number");
		assert(typeof(dev.name) == "string","dev.name is not a string");
		
		var i = this.devices.push(dev)-1;
		if(this.devices[i].create) this.devices[i].create(this.devices[i]);
	}
}
module.exports = Mailbox;
