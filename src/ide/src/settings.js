const electron = require("electron");
const fs = require("fs");
const path = require("path");
const paths = require("./paths");

var app = electron.app || electron.remote.app;

if(!fs.existsSync(path.join(paths["CFG"],"settings.json"))) {
	fs.writeFileSync(path.join(paths["CFG"],"settings.json"),JSON.stringify(require("./settings-defaults.json")));
}

function read() {
	return JSON.parse(fs.readFileSync(path.join(paths["CFG"],"settings.json")).toString());
}

function write(obj) {
	fs.writeFileSync(path.join(paths["CFG"],"settings.json"),JSON.stringify(obj));
}

var cfg = read();
for(var key in require("./settings-defaults.json")) {
	if(!cfg[key]) cfg[key] = require("./settings-defaults.json")[key];
}
write(cfg);

module.exports.read = read;
module.exports.write = write;
