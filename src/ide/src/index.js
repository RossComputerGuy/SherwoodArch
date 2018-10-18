import { app, BrowserWindow, Menu, MenuItem } from "electron";
const logger = require("./logger");

if(require("electron-squirrel-startup")) { // eslint-disable-line global-require
	app.quit();
}
let mainWindow;

const createWindow = () => {
	mainWindow = new BrowserWindow({
		width: 800,
		height: 600
	});
	logger.info("Creating window");
	
	mainWindow.loadURL(`file://${__dirname}/index.html`);
	
	mainWindow.on("closed",() => {
		mainWindow = null;
		logger.info("Closing window");
	});
};

app.on("ready",createWindow);

app.on("window-all-closed",() => {
  if(process.platform !== "darwin") app.quit();
});

app.on("activate",() => {
	if(mainWindow === null) createWindow();
});
