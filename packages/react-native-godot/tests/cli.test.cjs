'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const http = require('node:http');
const os = require('node:os');
const path = require('node:path');
const {spawnSync} = require('node:child_process');
const {buildBundle, findEntry} = require('../cli/build.cjs');
const {downloadEditor, hostPlatform, resolveEditor} = require('../cli/editor.cjs');
const {addMainScene, packageName, writeTemplate} = require('../cli/init.cjs');
const {addProjectSettings, ignoreNodeModules, loadGodotProject, readProjectSettings} = require('../cli/project.cjs');
const {importProject, runEditor} = require('../cli/run.cjs');

function godotProject(settings) {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'rn-godot-cli-'));
  fs.writeFileSync(path.join(root, 'project.godot'), settings);
  return root;
}

test('project settings are read from project.godot with Godot defaults', () => {
  const root = godotProject('config_version=5\n\n[react_native]\n\nproject/root="res://ui"\ndev_server/enabled=true\ndev_server/port=9000\n');
  const project = loadGodotProject(path.join(root, 'ui', 'components'));
  assert.equal(project.godotRoot, root);
  assert.equal(project.jsRoot, path.join(root, 'ui'));
  assert.equal(project.bundlePath, path.join(root, 'dist', 'godot.bundle.js'));
  assert.equal(project.entry, 'index');
  assert.deepEqual(project.devServer, {enabled: true, host: 'localhost', port: 9000});
  assert.equal(loadGodotProject(os.tmpdir()), null);
});

test('adding settings keeps existing values and other lines', () => {
  const root = godotProject('; comment\nconfig_version=5\n\n[application]\n\nconfig/name="Game"\n\n[react_native]\n\ndev_server/enabled=false\n\n[rendering]\n\nx=1\n');
  addProjectSettings(root, {
    'react_native/dev_server/enabled': true,
    'react_native/project/root': 'res://react_native/',
    'application/run/main_scene': 'res://Main.tscn',
    'display/window/size/viewport_width': 800,
  });
  const text = fs.readFileSync(path.join(root, 'project.godot'), 'utf8');
  assert.match(text, /^; comment\n/);
  assert.match(text, /\[react_native\]\n\ndev_server\/enabled=false\nproject\/root="res:\/\/react_native\/"\n\n\[rendering\]/);
  assert.match(text, /config\/name="Game"\nrun\/main_scene="res:\/\/Main.tscn"\n/);
  assert.match(text, /\[display\]\n\nwindow\/size\/viewport_width=800\n$/);
  assert.equal(readProjectSettings(root)['react_native/dev_server/enabled'], false);
});

test('entries resolve with or without their extension', () => {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'rn-godot-entry-'));
  fs.writeFileSync(path.join(root, 'index.tsx'), '');
  fs.writeFileSync(path.join(root, 'godot.entry.js'), '');
  assert.equal(findEntry(root, 'index'), 'index.tsx');
  assert.equal(findEntry(root, 'godot.entry'), 'godot.entry.js');
  assert.equal(findEntry(root, 'godot.entry.js'), 'godot.entry.js');
  assert.throws(() => findEntry(root, 'missing'), /No entry file/);
});

test('the template refuses to overwrite files and fills its placeholders', () => {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'rn-godot-template-'));
  const values = {name: 'game', react: '1', reactNative: '2', reactNativeGodot: 'file:vendor/x.tgz', typesReact: '3', typescript: '4'};
  writeTemplate(root, values);
  const manifest = JSON.parse(fs.readFileSync(path.join(root, 'package.json'), 'utf8'));
  assert.equal(manifest.dependencies['react-native-godot'], 'file:vendor/x.tgz');
  assert.ok(fs.existsSync(path.join(root, '.gitignore')));
  assert.ok(!fs.existsSync(path.join(root, 'Main.tscn')));
  assert.throws(() => writeTemplate(root, values), /already exist/);
  assert.equal(packageName('/games/My Game!'), 'my-game-');
});

test('a main scene is added only when the project has none', () => {
  const root = godotProject('config_version=5\n');
  addMainScene(root);
  assert.equal(readProjectSettings(root)['application/run/main_scene'], 'res://Main.tscn');
  assert.match(fs.readFileSync(path.join(root, 'Main.tscn'), 'utf8'), /type="ReactNativeRootView"/);
  const configured = godotProject('[application]\n\nrun/main_scene="res://Game.tscn"\n');
  addMainScene(configured);
  assert.ok(!fs.existsSync(path.join(configured, 'Main.tscn')));
});

test('node_modules is hidden from Godot once it exists', () => {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'rn-godot-ignore-'));
  ignoreNodeModules(root);
  assert.ok(!fs.existsSync(path.join(root, 'node_modules')));
  fs.mkdirSync(path.join(root, 'node_modules'));
  ignoreNodeModules(root);
  assert.ok(fs.existsSync(path.join(root, 'node_modules', '.gdignore')));
});

test('an explicit editor takes precedence and must exist', async () => {
  const editor = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'rn-godot-editor-')), 'godot');
  fs.writeFileSync(editor, '');
  assert.equal(await resolveEditor({godot: editor}), editor);
  await assert.rejects(resolveEditor({godot: editor + '-missing'}), /not found/);
});

test('the prebuilt editor is downloaded once and checked against the package', async t => {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'rn-godot-download-'));
  const packageDir = path.join(root, 'react-native-godot-linux-x86_64');
  fs.mkdirSync(packageDir);
  fs.writeFileSync(path.join(packageDir, 'godot.linuxbsd.editor.dev.x86_64'), '');
  const archivePath = path.join(root, 'editor.zip');
  assert.equal(spawnSync('python3', ['-m', 'zipfile', '-c', archivePath, packageDir]).status, 0);
  const archive = fs.readFileSync(archivePath);
  let requests = 0;
  const server = http.createServer((_request, response) => {
    requests += 1;
    response.end(archive);
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  t.after(() => server.close());
  t.after(() => server.closeAllConnections());
  const cacheHome = process.env.XDG_CACHE_HOME;
  process.env.XDG_CACHE_HOME = path.join(root, 'cache');
  t.after(() => (cacheHome == null ? delete process.env.XDG_CACHE_HOME : (process.env.XDG_CACHE_HOME = cacheHome)));
  t.mock.method(console, 'info', () => {});

  const url = `http://127.0.0.1:${server.address().port}/editor.zip`;
  const sha256 = crypto.createHash('sha256').update(archive).digest('hex');
  const platform = commit => ({bindingsCommit: commit, editors: {[hostPlatform()]: {url, sha256}}});
  const editor = await downloadEditor(platform('first'));
  assert.equal(path.basename(editor), 'godot.linuxbsd.editor.dev.x86_64');
  assert.ok(fs.statSync(editor).mode & 0o100);
  assert.equal(await downloadEditor(platform('first')), editor);
  assert.equal(requests, 1);
  await assert.rejects(
    downloadEditor({bindingsCommit: 'second', editors: {[hostPlatform()]: {url, sha256: '0'.repeat(64)}}}),
    /changed since this package was built.*Update the vendored react-native-godot package, or pass --godot/,
  );
});

test('an editor that cannot start or import fails with a clear error', async () => {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'rn-godot-broken-editor-'));
  const notExecutable = path.join(root, 'godot');
  fs.writeFileSync(notExecutable, '');
  await assert.rejects(runEditor(notExecutable, []), /Could not start the Godot editor .*godot: spawn .* EACCES/);
  const failing = path.join(root, 'failing-godot');
  fs.writeFileSync(failing, '#!/bin/sh\nexit 3\n', {mode: 0o755});
  assert.throws(() => importProject(failing, root), /Godot could not import .*: exit code 3/);
});

test('a bundle cannot share its directory with the JavaScript project', async () => {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'rn-godot-bundle-'));
  const jsRoot = path.join(root, 'react_native');
  for (const out of [path.join(root, 'game.bundle.js'), 'index.bundle.js']) {
    await assert.rejects(buildBundle({projectRoot: jsRoot, entry: 'index', out}), /must be in a directory of its own/);
  }
});
