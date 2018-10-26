const assert = require("assert");
const jints = require("jints");
const x11 = require("x11");

const FLAGS = {
	"ENABLE": (1 << 0),
	"SETRES": (1 << 1)
};

const BITS_EXTRACT = (value,begin,size) => ((value >> begin) & (1 << ((begin+size)-begin))-1);

class Canvas2DGraphics {
	constructor(opts = {}) {
		this.hdr = {
			vendorID: 0xA900,
			deviceID: 0x0002,
			rev: 0,
			type: 4,
			classCode: 0,
			subclass: 0
		};
		
		assert(typeof(opts.canvas) == "object","opts.canvas must be an object");
		if(!opts.width) opts.width = 640;
		if(!opts.height) opts.height = 480;
		this.opts = opts;
		
		this.name = "Basic 2D Graphics Card on HTML Canvas";
		
		this.flags = 0;
		this.width = this.opts.width;
		this.height = this.opts.height;
		this.pos = 0;
		
		this.ctx = this.opts.canvas.getContext;
	}
	create(dev,vm) {
		this.write(dev,0,~FLAGS["ENABLE"],vm);
	}
	destroy(dev) {}
	cycle(dev,vm) {
		if(this.flags & FLAGS["ENABLE"]) this.ctx.putImageData(this.framebuffer,0,0);
	}
	read(dev,i,vm) {
		if(!(this.flags & FLAGS["ENABLE"])) return 0;
		switch(i) {
			case 0: return this.flags;
			case 1: return this.pos;
			case 2:
				var buff = Buffer.alloc(8);
				for(var x = 0;x < buff.length;x++) buff[x] = this.framebuffer[this.pos+x];
				var arr = new Int32Array(new Uint8Array(buff.slice(i,i+8)).buffer);
				var v = new jints.UInt64(0).join(arr[1],arr[0]);
				return parseInt(v.toString());
		}
	}
	write(dev,i,v,vm) {
		switch(i) {
			case 0:
				this.flags = v;
				if(v & FLAGS["ENABLE"]) {
					if(v & FLAGS["SETRES"]) {
						this.width = BITS_EXTRACT(this.flags,1,16);
						this.height = BITS_EXTRACT(this.flags,17,16);
					}
					this.framebuffer = this.ctx.createImageData(this.width,this.height);
					this.framebuffer.fill(0);
					this.ctx.putImageData(this.framebuffer,0,0);
					this.pos = 0;
				} else {
					this.pos = 0;
					this.width = this.opts.width;
					this.height = this.opts.height;
					delete this.framebuffer;
				}
				break;
			case 1:
				if(this.flags & FLAGS["ENABLE"]) this.pos = v;
				break;
			case 2:
				if(this.flags & FLAGS["ENABLE"]) {
					v = new jints.UInt64(v).toArray();
					for(var x = 0;x < v.length;x++) this.framebuffer[this.pos+x] = v[x];
				}
				break;
		}
	}
}
module.exports = Canvas2DGraphics;
