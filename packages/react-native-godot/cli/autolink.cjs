'use strict';

const crypto = require('crypto');
const fs = require('fs');
const os = require('os');
const path = require('path');
const {spawnSync} = require('child_process');
const {generateLibrary} = require('./codegen.cjs');
const {cacheRoot, readPlatform} = require('./editor.cjs');
const {readProjectSettings} = require('./project.cjs');

const LOCK_FILE = 'godot-autolinking.lock.json';

function autolinkingDir(jsRoot) {
  return path.join(jsRoot, 'node_modules', '.cache', 'react-native-godot', 'autolinking');
}

function manifestPath(jsRoot) {
  return path.join(autolinkingDir(jsRoot), 'autolinking.json');
}

function readJson(file, fallback) {
  return fs.existsSync(file) ? JSON.parse(fs.readFileSync(file, 'utf8')) : fallback;
}

// A project that never linked a native module has no manifest.
function readManifest(jsRoot) {
  return readJson(manifestPath(jsRoot), {hash: null, modules: [], codegen: []});
}

// The editor built for this exact set of native modules, when one exists.
function linkedEditor(manifest) {
  const directory = path.join(cacheRoot(), 'editors', manifest.hash);
  if (!fs.existsSync(directory)) {
    return null;
  }
  const binary = fs.readdirSync(directory).find(name => /^godot\..*\.editor\./.test(name));
  return binary ? path.join(directory, binary) : null;
}

// What a library declares under dependency.platforms.godot in its
// react-native.config.js, resolved against the library's directory.
function dependencyConfig(root, userConfig) {
  if (userConfig == null) {
    return null;
  }
  const manifest = JSON.parse(fs.readFileSync(path.join(root, 'package.json'), 'utf8'));
  const resolve = relative => (relative ? path.resolve(root, relative) : null);
  return {
    root,
    version: manifest.version,
    module: resolve(userConfig.module),
    addon: resolve(userConfig.addon),
    codegen: manifest.codegenConfig ? {name: manifest.codegenConfig.name, sources: path.resolve(root, manifest.codegenConfig.jsSrcsDir)} : null,
  };
}

function linkedLibraries(config) {
  return Object.entries(config.dependencies)
    .filter(([, dependency]) => dependency.platforms?.godot != null)
    .map(([name, dependency]) => ({name, ...dependency.platforms.godot}))
    .sort((a, b) => a.name.localeCompare(b.name));
}

function treeFiles(root) {
  return fs
    .readdirSync(root, {recursive: true, withFileTypes: true})
    // Godot imports a module's config.py, which leaves __pycache__ beside it.
    .filter(entry => entry.isFile() && !entry.parentPath.includes('__pycache__'))
    .map(entry => path.relative(root, path.join(entry.parentPath, entry.name)))
    .sort();
}

function hashTree(hash, root) {
  for (const file of treeFiles(root)) {
    hash.update(file + '\0');
    hash.update(fs.readFileSync(path.join(root, file)));
  }
}

// Godot writes .uid and .import files beside an addon's files. Linked copies
// keep them, so the project's references to the addon stay valid.
function addonFiles(root, source) {
  return treeFiles(root).filter(file => !/\.(uid|import)$/.test(file) || fs.existsSync(path.join(source, file)));
}

function sameAddon(source, copy) {
  if (!fs.existsSync(copy)) {
    return false;
  }
  const files = treeFiles(source);
  return JSON.stringify(files) === JSON.stringify(addonFiles(copy, source)) && files.every(file => fs.readFileSync(path.join(source, file)).equals(fs.readFileSync(path.join(copy, file))));
}

function copyAddon(source, copy) {
  if (fs.existsSync(copy)) {
    for (const file of addonFiles(copy, source)) {
      fs.rmSync(path.join(copy, file));
    }
  }
  fs.cpSync(source, copy, {recursive: true});
}

// Native modules, and the codegen they compile against, go into the editor
// build. The hash names the editor built for exactly this set.
function nativeManifest(libraries, outputDir, staging) {
  const native = libraries.filter(library => library.module != null);
  const hash = crypto.createHash('sha256');
  hash.update(readPlatform()?.bindingsCommit ?? 'development');
  const codegen = [];
  for (const library of native) {
    hash.update(`\0${library.name}@${library.version}\0`);
    hashTree(hash, library.module);
    const generated = path.join(staging, library.name);
    if (library.codegen) {
      generateLibrary(library.codegen.name, library.codegen.sources, generated);
    }
    if (fs.existsSync(generated)) {
      hashTree(hash, generated);
      codegen.push(path.join(outputDir, 'codegen', library.name));
    }
  }
  return {hash: native.length > 0 ? hash.digest('hex') : null, modules: native.map(library => library.module), codegen};
}

function enabledPlugins(godotRoot) {
  const value = readProjectSettings(godotRoot)['editor_plugins/enabled'];
  return typeof value === 'string' ? [...value.matchAll(/"res:\/\/addons\/([^/"]+)\/plugin\.cfg"/g)].map(match => match[1]) : [];
}

// Plugins are enabled through the editor, so their _enable_plugin and
// _disable_plugin run as when a developer toggles them in Project Settings.
function setPlugins(editor, godotRoot, {enable = [], disable = []}) {
  if (enable.length === 0 && disable.length === 0) {
    return;
  }
  const args = ['--headless', '--editor', '--path', godotRoot];
  if (enable.length > 0) {
    args.push('--react-native-enable-plugins=' + enable.join(','));
  }
  if (disable.length > 0) {
    args.push('--react-native-disable-plugins=' + disable.join(','));
  }
  const result = spawnSync(editor, args, {stdio: 'inherit'});
  if (result.status !== 0) {
    throw new Error(`Changing the enabled Godot plugins failed: ${result.error?.message ?? `exit code ${result.status ?? result.signal}`}`);
  }
}

function addonDirectory(library) {
  return path.basename(library.addon);
}

// Lists what differs from a fresh autolink, without changing anything.
function staleOutputs(project, jsRoot, libraries, manifest) {
  const problems = [];
  if (JSON.stringify(readManifest(jsRoot)) !== JSON.stringify(manifest)) {
    problems.push('the native module manifest is out of date');
  }
  const addons = libraries.filter(library => library.addon != null);
  const locked = readJson(path.join(jsRoot, LOCK_FILE), {addons: {}}).addons;
  const enabled = enabledPlugins(project.godotRoot);
  for (const library of addons) {
    const directory = addonDirectory(library);
    if (!sameAddon(library.addon, path.join(project.godotRoot, 'addons', directory))) {
      problems.push(`res://addons/${directory} differs from ${library.name}`);
    }
    if (!enabled.includes(directory)) {
      problems.push(`the ${directory} plugin is not enabled`);
    }
  }
  for (const directory of Object.keys(locked)) {
    if (!addons.some(library => addonDirectory(library) === directory)) {
      problems.push(`res://addons/${directory} belongs to a library that is no longer installed`);
    }
  }
  return problems;
}

// Links the project's React Native libraries into Godot: codegen and a
// manifest for the native modules an editor build compiles, and copies of
// their addons, enabled in project.godot.
async function autolink(project, config, {check = false, godot} = {}) {
  const jsRoot = config.root;
  const outputDir = autolinkingDir(jsRoot);
  const libraries = linkedLibraries(config);
  const staging = fs.mkdtempSync(path.join(os.tmpdir(), 'react-native-godot-codegen-'));
  try {
    const manifest = nativeManifest(libraries, outputDir, staging);
    if (check) {
      const problems = staleOutputs(project, jsRoot, libraries, manifest);
      if (manifest.hash != null && godot == null && process.env.GODOT_EDITOR == null && linkedEditor(manifest) == null) {
        problems.push('no editor is built for the linked native modules; run react-native build-godot-editor');
      }
      return problems;
    }
    if (JSON.stringify(readManifest(jsRoot)) !== JSON.stringify(manifest)) {
      fs.rmSync(path.join(outputDir, 'codegen'), {recursive: true, force: true});
      fs.mkdirSync(outputDir, {recursive: true});
      fs.cpSync(staging, path.join(outputDir, 'codegen'), {recursive: true});
      fs.writeFileSync(manifestPath(jsRoot), JSON.stringify(manifest, null, 2) + '\n');
    }
    await linkAddons(project, jsRoot, libraries, godot);
    return [];
  } finally {
    fs.rmSync(staging, {recursive: true, force: true});
  }
}

async function linkAddons(project, jsRoot, libraries, godot) {
  const lockPath = path.join(jsRoot, LOCK_FILE);
  const locked = readJson(lockPath, {addons: {}}).addons;
  const addons = libraries.filter(library => library.addon != null);
  const current = Object.fromEntries(addons.map(library => [addonDirectory(library), library.name]));
  const removed = Object.keys(locked).filter(directory => !(directory in current));
  const enabled = enabledPlugins(project.godotRoot);
  const toEnable = Object.keys(current).filter(directory => !enabled.includes(directory));
  const toDisable = removed.filter(directory => enabled.includes(directory));
  for (const directory of Object.keys(current)) {
    if (!(directory in locked) && fs.existsSync(path.join(project.godotRoot, 'addons', directory))) {
      throw new Error(`res://addons/${directory} already exists and was not linked by autolink-godot. Remove it to link ${current[directory]}.`);
    }
  }
  let editor = null;
  // A plugin may use classes of the linked native modules, so their editor is
  // preferred; before it is built, the plugins load in the prebuilt editor.
  const {resolveEditor} = require('./editor.cjs');
  const resolve = async () => (editor ??= await resolveEditor({godot, project}).catch(() => resolveEditor({godot})));
  if (toDisable.length > 0) {
    setPlugins(await resolve(), project.godotRoot, {disable: toDisable});
  }
  for (const directory of removed) {
    fs.rmSync(path.join(project.godotRoot, 'addons', directory), {recursive: true, force: true});
  }
  for (const library of addons) {
    const destination = path.join(project.godotRoot, 'addons', addonDirectory(library));
    if (!sameAddon(library.addon, destination)) {
      copyAddon(library.addon, destination);
    }
  }
  if (Object.keys(current).length > 0 || Object.keys(locked).length > 0) {
    fs.writeFileSync(lockPath, JSON.stringify({addons: current}, null, 2) + '\n');
  }
  if (toEnable.length > 0) {
    setPlugins(await resolve(), project.godotRoot, {enable: toEnable});
  }
}

module.exports = {autolink, autolinkingDir, dependencyConfig, linkedEditor, manifestPath, readManifest};
