const FLAGS = {
	"ENABLE": (1 << 0)
};

const STATUS = {
	"PLAYING": (1 << 0)
};

const BITS_EXTRACT = (value,begin,size) => ((value >> begin) & (1 << ((begin+size)-begin))-1);

class MouseHIDDevice {
	constructor(vm) {
		super();
		this.hdr = {
			vendorID: 0xA900,
			deviceID: 0x0005,
			rev: 0,
			type: 5,
			classCode: 0,
			subclass: 0
		};
		
		this.name = "Basic Sound Card for Websites";
		
		this.flags = 0;
	}
	create(dev,vm) {
		this.ctx = new (window.AudioContext || window.webkitAudioContext)();
		this.audioBuffer = {
			address: 0,
			size: 0,
			sizePerChannel: 0,
			index: 0
		};
		this.source = this.ctx.createBufferSource();
		this.status = 0;
	}
	destroy(dev) {
		this.ctx.close();
	}
	cycle(dev,vm) {
		if(this.flags & FLAGS["ENABLE"]) {
			this.audioBuffer.index = this.ctx.currentTime;
		}
	}
	read(dev,i,vm) {
		if(!(this.flags & FLAGS["ENABLE"])) return 0;
		switch(i) {
			case 0: return this.flags;
			case 1: return this.audioBuffer.address;
			case 2: return this.audioBuffer.size;
			case 3: return this.audioBuffer.sizePerChannel;
			case 4: return this.audioBuffer.index;
			case 5: return this.status;
		}
		return 0;
	}
	write(dev,i,v,vm) {
		switch(i) {
			case 0:
				this.flags = v;
				if(v & FLAGS["ENABLE"]) {
					this.audioBuffer.address = 0;
					this.audioBuffer.size = 0;
					this.audioBuffer.sizePerChannel = 0;
					this.audioBuffer.index = 0;
				}
				break;
			case 1:
				this.audioBuffer.address = v; 
				break;
			case 2:
				this.audioBuffer.size = v;
				break;
			case 3:
				this.audioBuffer.sizePerChannel = v;
				break;
			case 4:
				this.audioBuffer.index = v;
				this.ctx.currentTime = this.audioBuffer.index;
				break;
			case 5:
				if(BITS_EXTRACT(v,0,1)) {
					/* Play */
					this.status |= STATUS["PLAYING"];
					this.audioBuffer.buffer = this.ctx.createBuffer(BITS_EXTRACT(v,1,8),this.audioBuffer.sizePerChannel,this.ctx.sampleRate);
					for(var x = 0;x < this.audioBuffer.buffer.numberOfChannels;x++) {
						var channel = this.audioBuffer.buffer.getChannelData(x);
						for(var z = 0;z < channel.length;z++) {
							channel[z] = vm.read(this.audioBuffer.address+(this.audioBuffer.sizePerChannel*x));
						}
					}
					this.source.buffer = this.audioBuffer.buffer;
					this.ctx.currentTime = this.audioBuffer.index;
					this.source.connect(this.ctx.destination);
					this.source.start();
				} else {
					/* Pause */
					this.status &= ~STATUS["PLAYING"];
					this.source.disconnect();
					this.source.stop();
					delete this.audioBuffer.buffer;
				}
				break;
		}
	}
}
module.exports = MouseHIDDevice;
