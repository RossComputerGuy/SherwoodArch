const electron = require("electron");
const fs = require("fs");
const path = require("path");

var app = electron.app || electron.remote.app;

const APP_PATH = path.join(app.getPath("appData"),"saide");

const PATHS = {
	"APP": APP_PATH,
	"CFG": path.join(APP_PATH,"config"),
	"LOGS": path.join(APP_PATH,"logs"),
	"WORKSPACE": path.join(APP_PATH,"workspace")
};

for(var p in PATHS) {
	if(!fs.existsSync(PATHS[p])) fs.mkdirSync(PATHS[p]);
}

module.exports = PATHS;
