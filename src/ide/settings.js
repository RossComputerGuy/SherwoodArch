const {app} = require("electron");
const fs = require("fs");
const path = require("path");

const APP_PATH = path.join(app.getPath("appData"),"saide");
const CFG_PATH = path.join(APP_PATH,"config");

function read() {
	return JSON.parse(fs.readFileSync(path.join(CFG_PATH,"settings.json").toString());
}

function write(obj) {
	fs.writeFileSync(path.join(CFG_PATH,"settings.json"),JSON.stringify(obj));
}

module.exports.read = read;
module.exports.write = write;
