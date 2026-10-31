'use strict';

const fs = require('fs');
const path = require('path');
const {spawnSync} = require('child_process');
const {manifestPath, readManifest} = require('./autolink.cjs');
const {cacheRoot, readPlatform} = require('./editor.cjs');

function run(command, args, options) {
  const result = spawnSync(command, args, {stdio: 'inherit', ...options});
  if (result.status !== 0) {
    throw new Error(`${command} ${args.join(' ')} failed: ${result.error?.message ?? `exit code ${result.status ?? result.signal}`}`);
  }
}

// The bindings source this package was released from, cloned once per commit.
function bindingsSource(platform) {
  if (platform?.repository == null) {
    throw new Error('This react-native-godot has no recorded bindings source. Pass --source <bindings checkout>.');
  }
  const directory = path.join(cacheRoot(), 'source', platform.bindingsCommit);
  if (!fs.existsSync(path.join(directory, '.git'))) {
    fs.rmSync(directory, {recursive: true, force: true});
    run('git', ['clone', '--filter=blob:none', '--no-checkout', platform.repository, directory]);
    run('git', ['-C', directory, 'checkout', '--detach', platform.bindingsCommit]);
  }
  return directory;
}

// Builds a Godot editor with the project's linked native modules and keeps it
// where run-godot and the other commands find it.
function buildGodotEditor(project, {source} = {}) {
  if (process.platform !== 'linux') {
    throw new Error('react-native build-godot-editor only builds Linux editors.');
  }
  const manifest = readManifest(project.jsRoot);
  if (manifest?.hash == null) {
    throw new Error('The project links no native Godot modules. Run react-native autolink-godot after installing a library that has one.');
  }
  const sourceDir = source ? path.resolve(source) : bindingsSource(readPlatform());
  const env = {...process.env, RN_AUTOLINKING: manifestPath(project.jsRoot)};
  run('bash', [path.join(sourceDir, 'scripts', 'bootstrap.sh')], {cwd: sourceDir, env});
  run('bash', [path.join(sourceDir, 'scripts', 'build_hermes.sh')], {cwd: sourceDir, env});
  // Warnings in a library's sources shouldn't stop its users' editor builds.
  run('bash', [path.join(sourceDir, 'scripts', 'build_godot.sh'), 'werror=no'], {cwd: sourceDir, env});
  const binDir = path.join(sourceDir, 'godot', 'bin');
  const editor = fs.readdirSync(binDir).find(name => /^godot\.linuxbsd\.editor\./.test(name));
  const destination = path.join(cacheRoot(), 'editors', manifest.hash);
  fs.rmSync(destination, {recursive: true, force: true});
  fs.mkdirSync(destination, {recursive: true});
  for (const file of [editor, 'libhermesvm.so', 'libjsi.so']) {
    fs.copyFileSync(path.join(binDir, file), path.join(destination, file));
  }
  fs.chmodSync(path.join(destination, editor), 0o755);
  return path.join(destination, editor);
}

module.exports = {buildGodotEditor};
