#!/usr/bin/env node
'use strict';

// Builds a Godot project's bundle for tools that can't set a working
// directory, such as the Godot editor when it exports.
const {parseArgs} = require('util');
const {buildProject} = require('../cli/build.cjs');
const {loadGodotProject} = require('../cli/project.cjs');

const {values} = parseArgs({
  options: {
    'project-root': {type: 'string'},
    dev: {type: 'string', default: 'false'},
    minify: {type: 'string', default: 'true'},
  },
});

const project = loadGodotProject(values['project-root']);
if (project == null) {
  console.error(`No Godot project contains ${values['project-root']}.`);
  process.exit(1);
}
buildProject(project, {dev: values.dev === 'true', minify: values.minify === 'true'})
  .then(output => console.info('Built ' + output))
  .catch(error => {
    console.error(error.stack ?? error);
    process.exitCode = 1;
  });
