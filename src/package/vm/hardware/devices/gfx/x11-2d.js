const jints = require("jints")
const x11 = require("x11");

const FLAGS = {
	"ENABLE": (1 << 0),
	"SETRES": (1 << 1)
};

const BITS_EXTRACT = (value,begin,size) => ((value >> begin) & (1 << ((begin+size)-begin))-1);

class X11Graphics2DDevice {
	constructor(opts = {}) {
		this.hdr = {
			vendorID: 0xA900,
			deviceID: 0x0001,
			rev: 0,
			type: 4,
			classCode: 0,
			subclass: 0
		};
		
		if(!opts.display) opts.display = ":0";
		if(!opts.width) opts.width = 640;
		if(!opts.height) opts.height = 480;
		if(!opts.bpp) opts.bpp = 4;
		
		this.name = "Basic 2D Graphics Card on X11 (DISPLAY: "+opts.display+")";
		
		this.flags = 0;
		
		x11.createClient({
			display: opts.display
		},(err,display) => {
			if(err) throw err;
			this.xdisplay = display;
			this.X = this.xdisplay.client;
			this.xids = {};
			this.xtensions = {};
			this.width = this.opts.width;
			this.height = this.opts.height;
			this.bpp = this.opts.bpp;
			this.pos = 0;
			this.X.require("render",(err,render) => {
				if(err) throw err;
				this.xtensions["render"] = render;
				this.X.on("event",ev => {
					if(ev.name == "Expose") this.xtensions["render"].Composite(3,this.xids["fbPicture"],0,this.xids["winPicture"],0,0,0,0,0,0,this.width,this.height);
				});
			});
		});
	}
	create(dev,vm) {
		this.write(dev,0,~FLAGS["ENABLE"],vm);
	}
	destroy(dev) {}
	cycle(dev,vm) {
		if(this.flags & FLAGS["ENABLE"]) {
			this.X.PutImage(2,this.xids["pixmap"],this.xids["gc"],this.width,this.height,0,0,0,this.bpp,this.framebuffer);
		}
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
						this.bpp = BITS_EXTRACT(this.flags,33,8);
					}
					this.framebuffer = Buffer.alloc(this.width*this.height*this.bpp);
					this.xids["win"] = this.X.AllocID();
					this.X.CreateWindow(
						this.xids["win"],this.xdisplay.screen[0].root,
						0,0,this.width,this.height,
						0,0,0,0,
						{ eventMask: x11.eventMask.Exposure }
					);
					this.X.MapWindow(this.xids["win"]);
					this.xids["gc"] = this.X.AllocID();
					this.X.CreateGC(this.xids["gc"],this.xids["win"]);
					this.xids["pixmap"] = this.X.AllocID();
					this.X.CreatePixmap(this.xids["pixmap"],this.xids["win"],this.bpp,this.width,this.height);
					this.xids["fbPicture"] = this.X.AllocID();
					this.xtensions["render"].CreatePicture(this.xids["fbPicture"],this.xids["pixmap"],this.xtensions["render"]["rgb"+this.bpp]);
					this.xids["winPicture"] = this.X.AllocID();
					this.xtensions["render"].CreatePicture(this.xids["fbPicture"],this.xids["win"],this.xtensions["render"]["rgb"+this.bpp]);
					this.pos = 0;
				} else {
					this.pos = 0;
					this.width = this.opts.width;
					this.height = this.opts.height;
					this.X.DestroyWindow(this.xids["win"]);
					for(var name in this.xids) this.X.ReleaseID(this.xids[name]);
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
module.exports = X11Graphics2DDevice;
