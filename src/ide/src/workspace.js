const fs = require("fs");
const path = require("path");
const paths = require("./paths");
const Project = require("./project");

function getProjectNames() {
	var names = [];
	for(var id of fs.readdirSync(paths["WORKSPACE"])) {
		var p = path.join(paths["WORKSPACE"],id);
		var projJSON = JSON.parse(fs.readFileSync(path.join(p,"project.json")).toString());
		names.push(projJSON["name"]);
	}
	return names;
}
module.exports.getProjectNames = getProjectNames;

function createProject(p,opts) {
	var proj = new Project({
		path: p,
		name: opts.name,
		description: opts.description
	});
	proj.save();
	return proj;
}
module.exports.createProject = createProject;
