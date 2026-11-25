'use strict';

const crypto = require('crypto');
const fs = require('fs');
const os = require('os');
const path = require('path');
const editor = require('./editor.cjs');

const TEMPLATE_ALTERNATIVES = 'Update the vendored react-native-godot package, or pass --file <tpz>.';

function exportTemplatesRoot() {
  const dataHome = process.env.XDG_DATA_HOME;
  const data = dataHome && path.isAbsolute(dataHome) ? dataHome : path.join(os.homedir(), '.local', 'share');
  return path.join(data, 'godot', 'export_templates');
}

async function downloadTemplates() {
  const release = editor.readPlatform()?.templates;
  if (release == null) {
    throw new Error('This react-native-godot has no released export templates. Pass --file <tpz>.');
  }
  console.info(`Downloading the React Native Godot export templates from ${release.url}`);
  const response = await fetch(release.url);
  if (!response.ok) {
    throw new Error(`Downloading the export templates from ${release.url} failed: HTTP ${response.status}. ${TEMPLATE_ALTERNATIVES}`);
  }
  const archive = Buffer.from(await response.arrayBuffer());
  const digest = crypto.createHash('sha256').update(archive).digest('hex');
  if (digest !== release.sha256) {
    throw new Error(`The export templates at ${release.url} changed since this package was built (sha256 ${digest}). ${TEMPLATE_ALTERNATIVES}`);
  }
  return archive;
}

async function installTemplates({file} = {}) {
  if (process.platform !== 'linux') {
    throw new Error('React Native Godot publishes Linux export templates only.');
  }
  const root = exportTemplatesRoot();
  fs.mkdirSync(root, {recursive: true});
  const staging = fs.mkdtempSync(path.join(root, '.react-native-godot-'));
  try {
    let archivePath = file && path.resolve(file);
    if (archivePath == null) {
      archivePath = path.join(staging, 'templates.tpz');
      fs.writeFileSync(archivePath, await downloadTemplates());
    }
    const extracted = path.join(staging, 'extracted');
    editor.extractZip(archivePath, extracted);
    const versionFile = path.join(extracted, 'templates', 'version.txt');
    if (!fs.existsSync(versionFile)) {
      throw new Error(`${archivePath} is not an export templates archive: it has no templates/version.txt.`);
    }
    const version = fs.readFileSync(versionFile, 'utf8').trim();
    if (!/^\w+\.\w+(\.\w+)+$/.test(version)) {
      throw new Error(`${archivePath} names an invalid template version: ${version}.`);
    }
    const destination = path.join(root, version);
    fs.rmSync(destination, {recursive: true, force: true});
    fs.renameSync(path.join(extracted, 'templates'), destination);
    return destination;
  } finally {
    fs.rmSync(staging, {recursive: true, force: true});
  }
}

module.exports = {exportTemplatesRoot, installTemplates};
