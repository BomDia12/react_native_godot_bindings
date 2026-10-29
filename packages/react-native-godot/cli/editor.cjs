'use strict';

const crypto = require('crypto');
const fs = require('fs');
const os = require('os');
const path = require('path');
const {spawnSync} = require('child_process');

const PLATFORM_FILE = path.join(__dirname, '..', 'godot-platform.json');
const EDITOR_ALTERNATIVES = 'Update the vendored react-native-godot package, or pass --godot <editor> or set GODOT_EDITOR.';

function cacheRoot() {
  return path.join(process.env.XDG_CACHE_HOME || path.join(os.homedir(), '.cache'), 'react-native-godot');
}

function hostPlatform() {
  const architectures = {x64: 'x86_64', arm64: 'arm64'};
  return `${process.platform}-${architectures[process.arch] ?? process.arch}`;
}

function readPlatform() {
  return fs.existsSync(PLATFORM_FILE) ? JSON.parse(fs.readFileSync(PLATFORM_FILE, 'utf8')) : null;
}

function findEditorBinary(directory) {
  for (const entry of fs.readdirSync(directory, {withFileTypes: true})) {
    const candidate = path.join(directory, entry.name);
    if (entry.isDirectory()) {
      const nested = findEditorBinary(candidate);
      if (nested) {
        return nested;
      }
    } else if (/^godot\..*\.editor\./.test(entry.name)) {
      return candidate;
    }
  }
  return null;
}

function extractZip(archive, destination) {
  const unzip = spawnSync('unzip', ['-q', archive, '-d', destination], {stdio: 'inherit'});
  if (unzip.error?.code === 'ENOENT') {
    const python = spawnSync('python3', ['-m', 'zipfile', '-e', archive, destination], {stdio: 'inherit'});
    if (python.status !== 0) {
      throw new Error('Extracting the Godot editor needs unzip or python3.');
    }
    return;
  }
  if (unzip.status !== 0) {
    throw new Error('Could not extract ' + archive);
  }
}

// Downloads the editor this package version was built with, once per machine.
async function downloadEditor(platform) {
  const release = platform.editors?.[hostPlatform()];
  if (release == null) {
    throw new Error(`No prebuilt React Native Godot editor for ${hostPlatform()}.`);
  }
  const directory = path.join(cacheRoot(), platform.bindingsCommit);
  const existing = fs.existsSync(directory) ? findEditorBinary(directory) : null;
  if (existing) {
    return existing;
  }
  console.info(`Downloading the React Native Godot editor from ${release.url}`);
  const response = await fetch(release.url);
  if (!response.ok) {
    throw new Error(`Downloading ${release.url} failed: HTTP ${response.status}. ${EDITOR_ALTERNATIVES}`);
  }
  const archive = Buffer.from(await response.arrayBuffer());
  const digest = crypto.createHash('sha256').update(archive).digest('hex');
  if (digest !== release.sha256) {
    throw new Error(`The editor archive at ${release.url} changed since this package was built (sha256 ${digest}). ${EDITOR_ALTERNATIVES}`);
  }
  const staging = fs.mkdtempSync(path.join(os.tmpdir(), 'react-native-godot-editor-'));
  try {
    const archivePath = path.join(staging, 'editor.zip');
    fs.writeFileSync(archivePath, archive);
    extractZip(archivePath, path.join(staging, 'editor'));
    fs.mkdirSync(path.dirname(directory), {recursive: true});
    fs.renameSync(path.join(staging, 'editor'), directory);
  } finally {
    fs.rmSync(staging, {recursive: true, force: true});
  }
  const binary = findEditorBinary(directory);
  fs.chmodSync(binary, 0o755);
  return binary;
}

// The editor a command runs: one named explicitly, then the editor built for
// the project's linked native modules, then the prebuilt editor of this
// package version.
async function resolveEditor({godot, project} = {}) {
  const explicit = godot ?? process.env.GODOT_EDITOR;
  if (explicit) {
    if (!fs.existsSync(explicit)) {
      throw new Error('Godot editor not found: ' + explicit);
    }
    return path.resolve(explicit);
  }
  const {linkedEditor, readManifest} = require('./autolink.cjs');
  const manifest = project ? readManifest(project.jsRoot) : null;
  if (manifest?.hash != null) {
    const editor = linkedEditor(manifest);
    if (editor == null) {
      throw new Error('This project links native Godot modules. Run react-native build-godot-editor, or pass --godot <editor>.');
    }
    return editor;
  }
  const platform = readPlatform();
  if (platform == null) {
    throw new Error('This react-native-godot has no prebuilt editor. Pass --godot <editor> or set GODOT_EDITOR.');
  }
  return downloadEditor(platform);
}

module.exports = {cacheRoot, downloadEditor, findEditorBinary, hostPlatform, readPlatform, resolveEditor};
