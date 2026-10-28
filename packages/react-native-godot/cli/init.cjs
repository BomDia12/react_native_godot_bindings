#!/usr/bin/env node
'use strict';

// Adds React Native to a Godot project: the JS project from the template, this
// package vendored into it, the Project Settings it needs, a first scene, and
// the editor this package was built for.
const fs = require('fs');
const path = require('path');
const {spawnSync} = require('child_process');
const {parseArgs} = require('util');
const {resolveEditor} = require('./editor.cjs');
const {addProjectSettings, ignoreNodeModules, readProjectSettings} = require('./project.cjs');

const PACKAGE_ROOT = path.join(__dirname, '..');
const TEMPLATE_ROOT = path.join(__dirname, 'template');
const RENAMED_TEMPLATE_FILES = {'_package.json': 'package.json', '_gitignore': '.gitignore'};

function packageName(directory) {
  const name = path.basename(directory).toLowerCase().replace(/[^a-z0-9._-]+/g, '-').replace(/^[._-]+/, '');
  return name || 'godot-react-native';
}

function run(command, args, options) {
  const result = spawnSync(command, args, {stdio: 'inherit', ...options});
  if (result.status !== 0) {
    throw new Error(`${command} ${args.join(' ')} failed`);
  }
  return result;
}

function packThisPackage(destination) {
  fs.mkdirSync(destination, {recursive: true});
  const result = run('npm', ['pack', PACKAGE_ROOT, '--pack-destination', destination, '--json'], {stdio: ['ignore', 'pipe', 'inherit']});
  return JSON.parse(result.stdout.toString())[0].filename;
}

function templateFiles() {
  return fs.readdirSync(TEMPLATE_ROOT).filter(file => file !== 'Main.tscn');
}

function writeTemplate(jsRoot, values) {
  const conflicts = templateFiles()
    .map(file => path.join(jsRoot, RENAMED_TEMPLATE_FILES[file] ?? file))
    .filter(file => fs.existsSync(file));
  if (conflicts.length > 0) {
    throw new Error('These files already exist; move them away and run again:\n' + conflicts.join('\n'));
  }
  fs.mkdirSync(jsRoot, {recursive: true});
  for (const file of templateFiles()) {
    const content = fs.readFileSync(path.join(TEMPLATE_ROOT, file), 'utf8').replace(/\{\{(\w+)\}\}/g, (_match, key) => values[key]);
    fs.writeFileSync(path.join(jsRoot, RENAMED_TEMPLATE_FILES[file] ?? file), content);
  }
}

function addMainScene(godotRoot) {
  if (readProjectSettings(godotRoot)['application/run/main_scene'] != null) {
    return;
  }
  const scene = path.join(godotRoot, 'Main.tscn');
  if (fs.existsSync(scene)) {
    console.warn('Main.tscn exists but is not the main scene; add a ReactNativeRootView to a scene yourself.');
    return;
  }
  fs.copyFileSync(path.join(TEMPLATE_ROOT, 'Main.tscn'), scene);
  addProjectSettings(godotRoot, {'application/run/main_scene': 'res://Main.tscn'});
}

async function main() {
  const {values: options} = parseArgs({
    options: {
      path: {type: 'string', default: '.'},
      root: {type: 'string', default: 'react_native'},
      godot: {type: 'string'},
    },
  });
  const godotRoot = path.resolve(options.path);
  const jsRoot = path.join(godotRoot, options.root);
  const projectFile = path.join(godotRoot, 'project.godot');
  if (!fs.existsSync(projectFile)) {
    fs.mkdirSync(godotRoot, {recursive: true});
    fs.writeFileSync(projectFile, `config_version=5\n\n[application]\n\nconfig/name=${JSON.stringify(path.basename(godotRoot))}\n`);
  }

  const manifest = JSON.parse(fs.readFileSync(path.join(PACKAGE_ROOT, 'package.json'), 'utf8'));
  writeTemplate(jsRoot, {
    name: packageName(godotRoot),
    react: manifest.peerDependencies.react,
    reactNative: manifest.peerDependencies['react-native'],
    reactNativeGodot: 'file:vendor/' + packThisPackage(path.join(jsRoot, 'vendor')),
    typesReact: manifest.devDependencies['@types/react'],
    typescript: manifest.devDependencies.typescript,
  });
  run('npm', ['install'], {cwd: jsRoot});
  ignoreNodeModules(jsRoot);

  const root = 'res://' + path.relative(godotRoot, jsRoot).replaceAll(path.sep, '/') + '/';
  addProjectSettings(godotRoot, {
    'react_native/project/root': root,
    'react_native/bundle/path': root + 'dist/index.bundle.js',
    'react_native/dev_server/enabled': true,
    'react_native/dev_server/entry': 'index',
  });
  addMainScene(godotRoot);

  const editor = await resolveEditor({godot: options.godot});
  console.info(`
React Native is ready in ${jsRoot}.
Godot editor: ${editor}

From ${jsRoot}:
  npx react-native run-godot           run the game
  npx react-native run-godot --editor  open the Godot editor
`);
}

if (require.main === module) {
  main().catch(error => {
    console.error(error.message);
    process.exitCode = 1;
  });
}

module.exports = {addMainScene, packageName, writeTemplate};
