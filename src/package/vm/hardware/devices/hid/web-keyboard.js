const EventEmitter = require("events");

const FLAGS = {
	"ENABLE": (1 << 0)
};

const KEYS = [
	/* Special Keys */
	"Unidentified",

	/* Modifier Keys */
	"Alt","AltGraph",
	"CapsLock",
	"Control",
	"F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12",
	"FnLock",
	"Hyper",
	"Meta",
	"NumLock",
	"ScrollLock",
	"Shift",
	"Super",
	"Symbol","SymbolLock",
	
	/* Whitespace Keys */
	"Enter","Tab"," ",
	
	/* Navigation Keys */
	"ArrowDown","ArrowLeft","ArrowRight","ArrowUp",
	"End","Home","PageUp","PageDown",
	
	/* Editing Keys */
	"Backspace",
	"Clear",
	"Copy",
	"CrSel",
	"Cut",
	"Delete",
	"Insert",
	
	/* Misc Keys */
	"Escape",
	
	/* Device */
	"BrightnessDown",
	"BrightnessUp",
	"Eject",
	"Power",
	"PrintScreen",
	
	/* Multimedia Keys */
	"ChannelDown",
	"ChannelUp",
	"MediaFastForward",
	"MediaPause",
	"MediaPlay",
	"MediaPlayPause",
	"MediaRecord",
	"MediaRewind",
	"MediaStop",
	"MediaTrackNext",
	"MediaTrackPrevious",
	
	/* Audio Control Keys */
	"AudioVolumeMute",
	"AudioVolumeUp",
	"AudioVolumeDown",
	
	/* Numeric Keypad Keys */
	"Key11",
	"Key12",
	"Clear",
	"Separator",

	/* Character Keys */
	"!","@","#","$","%","^","&","*","(",")","_","+",
	"1","2","3","4","5","6","7","8","9","0","-","=",
	
	"Q","W","E","R","T","Y","U","I","O","P","{","}","|",
	"q","w","e","r","t","y","u","i","o","p","[","]","\\",
	
	"A","S","D","F","G","H","J","K","L",":","\"",
	"a","s","d","f","g","h","j","k","l",";","'",
	
	"Z","X","C","V","B","N","M","<",">","?",
	"z","x","c","v","b","n","m",",",".","/"
];

class KeyboardHIDDevice extends EventEmitter {
	constructor(vm) {
		super();
		this.hdr = {
			vendorID: 0xA900,
			deviceID: 0x0003,
			rev: 0,
			type: 7,
			classCode: 0,
			subclass: 0
		};
		
		this.name = "Basic Keyboard Device";
		
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
		window.addEventListener("keyup",this.eventKeyUp,false);
		window.addEventListener("keydown",this.eventKeyDown,false);
	}
	destroy(dev) {
		window.removeEventListener("keyup",this.eventKeyUp,false);
		window.removeEventListener("keydown",this.eventKeyDown,false);
	}
	makeKeyInput(event,isUp) {
		var i = 0;
		if(event.key == "OS") event.key == "Super";
		if(KEYS.indexOf(event.key) > -1) i |= (KEYS.indexOf(event.key)+1) << 0;
		if(event.ctrlKey) i |= 1 << (KEYS.length+1);
		if(event.shiftKey) i |= 1 << (KEYS.length+2);
		if(event.altKey) i |= 1 << (KEYS.length+3);
		if(event.metaKey) i |= 1 << (KEYS.length+4);
		if(event.repeat) i |= 1 << (KEYS.length+5);
		if(isUp) i |= 1 << (KEYS.length+6);
		return i;
	}
	eventKeyUp(event) {
		if(!(this.flags & FLAGS["ENABLE"])) return;
		this.emit("input",this.makeKeyInput(event,true));
	}
	eventKeyDown(event) {
		if(!(this.flags & FLAGS["ENABLE"])) return;
		this.emit("input",this.makeKeyInput(event,false));
	}
	cycle(dev,vm) {
	}
	read(dev,i,vm) {
		if(!(this.flags & FLAGS["ENABLE"])) return 0;
		switch(i) {
			case 0: return this.flags;
			case 1: return this.input.length > 0;
			case 2: return this.input.shift() || 0;
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
module.exports = KeyboardHIDDevice;
