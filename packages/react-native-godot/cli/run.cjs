'use strict';

const fs = require('fs');
const path = require('path');
const {spawn, spawnSync} = require('child_process');
const {buildBundle} = require('./build.cjs');
const {resolveEditor} = require('./editor.cjs');
const {ignoreNodeModules} = require('./project.cjs');

const METRO_START_TIMEOUT_MS = 60000;

async function isMetroRunning({host, port}) {
  try {
    const response = await fetch(`http://${host}:${port}/status`, {signal: AbortSignal.timeout(1000)});
    return (await response.text()) === 'packager-status:running';
  } catch {
    return false;
  }
}

async function startMetro(project) {
  const metro = spawn(
    process.execPath,
    [path.join(__dirname, '..', 'scripts', 'start.cjs'), '--project-root', project.jsRoot, '--host', project.devServer.host, '--port', String(project.devServer.port)],
    {stdio: ['ignore', 'inherit', 'inherit']},
  );
  const deadline = Date.now() + METRO_START_TIMEOUT_MS;
  while (!(await isMetroRunning(project.devServer))) {
    if (metro.exitCode != null || Date.now() > deadline) {
      metro.kill();
      throw new Error('Metro did not start.');
    }
    await new Promise(resolve => setTimeout(resolve, 250));
  }
  return metro;
}

function importProject(editor, godotRoot) {
  const result = spawnSync(editor, ['--headless', '--editor', '--import', '--path', godotRoot], {stdio: 'inherit'});
  if (result.status !== 0) {
    throw new Error(`Godot could not import ${godotRoot}: ${result.error?.message ?? `exit code ${result.status ?? result.signal}`}`);
  }
}

function runEditor(editor, args) {
  return new Promise((resolve, reject) => {
    spawn(editor, args, {stdio: 'inherit'})
      .on('error', error => reject(new Error(`Could not start the Godot editor ${editor}: ${error.message}`)))
      .on('exit', code => resolve(code ?? 1));
  });
}

// Runs the game, or opens the editor, against Metro when the project uses the
// developer server and against a fresh bundle file otherwise.
async function runGodot(project, {packager = true, editor: openEditor = false, godot, godotArgs = []}) {
  ignoreNodeModules(project.jsRoot);
  const usesMetro = packager && project.devServer.enabled;
  let metro = null;
  if (usesMetro && !(await isMetroRunning(project.devServer))) {
    metro = await startMetro(project);
  } else if (!usesMetro) {
    await buildBundle({projectRoot: project.jsRoot, entry: project.entry, out: project.bundlePath});
  }
  try {
    const editor = await resolveEditor({godot});
    if (!fs.existsSync(path.join(project.godotRoot, '.godot'))) {
      importProject(editor, project.godotRoot);
    }
    const args = ['--path', project.godotRoot, ...(openEditor ? ['--editor'] : []), ...(usesMetro ? [] : ['--react-native-no-dev-server']), ...godotArgs];
    process.exitCode = await runEditor(editor, args);
  } finally {
    metro?.kill();
  }
}

module.exports = {importProject, isMetroRunning, runEditor, runGodot};
