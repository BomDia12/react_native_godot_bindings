'use strict';

const fs = require('fs');
const path = require('path');
const {loadGodotProject} = require('./project.cjs');

const parseBoolean = value => value !== 'false';

function requireProject(config) {
  const project = loadGodotProject(config.root);
  if (project == null) {
    throw new Error(`No Godot project (project.godot) contains ${config.root}.`);
  }
  return project;
}

const commands = [
  {
    name: 'build-godot',
    description: 'Builds the Godot bundle, its source map and its assets',
    options: [
      {name: '--entry <entry>', description: 'Entry file; defaults to react_native/dev_server/entry'},
      {name: '--out <file>', description: 'Bundle file; defaults to react_native/bundle/path'},
      {name: '--dev [boolean]', description: 'Build a development bundle', parse: parseBoolean, default: false},
      {name: '--minify [boolean]', description: 'Minify the bundle', parse: parseBoolean, default: false},
      {name: '--godot <editor>', description: 'Godot editor that regenerates component contracts; defaults to GODOT_EDITOR, then the prebuilt editor'},
    ],
    func: async (_argv, config, args) => {
      const project = requireProject(config);
      const {buildBundle} = require('./build.cjs');
      const {generateContracts} = require('./contracts.cjs');
      const {resolveEditor} = require('./editor.cjs');
      if (fs.existsSync(path.join(project.jsRoot, 'contracts'))) {
        generateContracts(project, await resolveEditor({godot: args.godot, project}));
      }
      const output = await buildBundle({
        projectRoot: config.root,
        entry: args.entry ?? project.entry,
        out: args.out ?? project.bundlePath,
        dev: args.dev,
        minify: args.minify,
      });
      console.info('Built ' + output);
    },
  },
  {
    name: 'run-godot [godot-args...]',
    description: 'Runs the Godot project, starting Metro when it uses the developer server. Arguments after -- go to Godot',
    options: [
      {name: '--no-packager', description: 'Build the bundle file instead of serving it from Metro'},
      {name: '--editor', description: 'Open the Godot editor instead of running the game'},
      {name: '--godot <editor>', description: 'Godot editor binary; defaults to GODOT_EDITOR, then the prebuilt editor'},
    ],
    func: async ([godotArgs], config, args) => {
      const {runGodot} = require('./run.cjs');
      await runGodot(requireProject(config), {...args, godotArgs});
    },
  },
  {
    name: 'autolink-godot',
    description: 'Links the native modules and addons of installed React Native Godot libraries into the project',
    options: [
      {name: '--check', description: 'Exit with an error when the linked outputs are out of date, without changing them'},
      {name: '--godot <editor>', description: 'Godot editor that enables linked plugins; defaults to GODOT_EDITOR, then the prebuilt editor'},
    ],
    func: async (_argv, config, args) => {
      const {autolink} = require('./autolink.cjs');
      const problems = await autolink(requireProject(config), config, args);
      if (problems.length > 0) {
        console.error('Autolinking is out of date. Run react-native autolink-godot.\n- ' + problems.join('\n- '));
        process.exitCode = 1;
      }
    },
  },
  {
    name: 'build-godot-editor',
    description: 'Builds a Linux Godot editor with the native modules that autolink-godot linked',
    options: [{name: '--source <checkout>', description: 'React Native Godot bindings checkout to build from; defaults to the commit this package was released from'}],
    func: async (_argv, config, args) => {
      const {autolink} = require('./autolink.cjs');
      const {buildGodotEditor} = require('./build-editor.cjs');
      const project = requireProject(config);
      const editor = buildGodotEditor(project, args);
      // Plugins of native libraries wait for this editor to be enabled.
      await autolink(project, config, {godot: editor});
      console.info('Built ' + editor);
    },
  },
];

function projectConfig(projectRoot) {
  return loadGodotProject(projectRoot) ?? undefined;
}

function dependencyConfig(root, userConfig) {
  return require('./autolink.cjs').dependencyConfig(root, userConfig);
}

module.exports = {commands, dependencyConfig, projectConfig};
