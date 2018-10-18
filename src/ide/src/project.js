const EventEmitter = require("events");
const fs = require("fs");
const logger = require("./logger");
const path = require("path");
const paths = require("./paths");

class Project extends EventEmitter {
	constructor(opts = {}) {
		super();
		this.opts = opts;
		if(this.opts.name) this.name = this.opts.name;
		if(this.opts.description) this.description = this.opts.description;
		
		if(!fs.existsSync(this.opts.path)) fs.mkdirSync(this.opts.path);
		this.loaded = false;
		
		this.watcher = fs.watch(this.getSourceDir(),{
			persistent: true,
			recursive: true
		},(type,filename) => {
			if(filename.endsWith(".part")) return;
			if(type == "rename") {
				if(fs.existsSync(path.join(this.getSourceDir(),filename.toString()))) {
					this.emit("file/created",path.join(this.getSourceDir(),filename.toString()));
				} else {
					this.emit("file/deleted",path.join(this.getSourceDir(),filename.toString()));
				}
			}
		});
	}
	load() {
		try {
			var projJSON = JSON.parse(fs.readFileSync(path.join(this.opts.path,"project.json")).toString());
			for(var key in projJSON) {
				if(key != "ideVersion") this[key] = projJSON[key];
			}
			this.loaded = true;
		} catch(e) {
			throw new Error("Failed to load project, try saving it first");
		}
	}
	save() {
		var projJSON = {
			name: this.name,
			description: this.description,
			ideVersion: require("../package.json").version
		};
		
		if(!fs.existsSync(this.opts.path)) fs.mkdirSync(this.opts.path);
		if(!fs.existsSync(path.join(this.getSourceDir(),"main.s"))) fs.writeFileSync(path.join(this.getSourceDir(),"main.s"),"main:\n\tjmp main");
		fs.writeFileSync(path.join(this.opts.path,"project.json"),JSON.stringify(projJSON));
		
		if(!fs.existsSync(this.getSourceDir())) fs.mkdirSync(this.getSourceDir());
		this.loaded = true;
	}
	unload() {
		this.loaded = false;
		this.watcher.close();
		delete this;
	}
	
	getSourceDir() {
		return path.join(this.opts.path,"src");
	}
	
	static fromPath(p) {
		if(!fs.existsSync(p)) throw new Error("No project is found at \""+p+"\"");
		var proj = new Project({
			path: p
		});
		proj.load();
		return proj;
	}
	
	static fromName(name) {
		for(var id of fs.readdirSync(paths["WORKSPACE"])) {
			var p = path.join(paths["WORKSPACE"],id);
			var projJSON = JSON.parse(fs.readFileSync(path.join(p,"project.json")).toString());
			if(projJSON["name"] == name) return Project.fromPath(p);
		}
		throw new Error("No project called \""+name+"\" exists");
	}
}
module.exports = Project;
