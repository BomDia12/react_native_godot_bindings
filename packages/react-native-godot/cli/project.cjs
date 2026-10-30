'use strict';

const fs = require('fs');
const path = require('path');

const SETTING_DEFAULTS = {
  'react_native/project/root': 'res://react_native/',
  'react_native/bundle/path': 'res://dist/godot.bundle.js',
  'react_native/dev_server/enabled': false,
  'react_native/dev_server/host': 'localhost',
  'react_native/dev_server/port': 8081,
  'react_native/dev_server/entry': 'index',
  'react_native/bundle/segments': 'PackedStringArray()',
};

function findGodotRoot(start) {
  for (let directory = path.resolve(start); ; directory = path.dirname(directory)) {
    if (fs.existsSync(path.join(directory, 'project.godot'))) {
      return directory;
    }
    if (path.dirname(directory) === directory) {
      return null;
    }
  }
}

// Reads the plain values this platform uses from Godot's ConfigFile syntax.
function parseValue(text) {
  const value = text.trim();
  if (value.startsWith('"') && value.endsWith('"')) {
    return JSON.parse(value);
  }
  if (value === 'true' || value === 'false') {
    return value === 'true';
  }
  const number = Number(value);
  return Number.isNaN(number) ? value : number;
}

function formatValue(value) {
  return typeof value === 'string' ? JSON.stringify(value) : String(value);
}

function readProjectSettings(godotRoot) {
  const settings = {};
  let section = '';
  for (const line of fs.readFileSync(path.join(godotRoot, 'project.godot'), 'utf8').split('\n')) {
    const header = /^\[([^\]]+)\]\s*$/.exec(line);
    const entry = /^([^=;#\s][^=]*)=(.*)$/.exec(line);
    if (header) {
      section = header[1];
    } else if (entry) {
      settings[section + '/' + entry[1].trim()] = parseValue(entry[2]);
    }
  }
  return settings;
}

// Adds settings that project.godot doesn't set yet, leaving every other line as
// it was, so a project the developer already configured keeps its choices.
function addProjectSettings(godotRoot, additions) {
  const file = path.join(godotRoot, 'project.godot');
  const existing = readProjectSettings(godotRoot);
  const lines = fs.readFileSync(file, 'utf8').replace(/\n*$/, '').split('\n');
  for (const [name, value] of Object.entries(additions)) {
    if (name in existing) {
      continue;
    }
    const section = name.slice(0, name.indexOf('/'));
    const entry = name.slice(section.length + 1) + '=' + formatValue(value);
    const start = lines.indexOf('[' + section + ']');
    if (start === -1) {
      lines.push('', '[' + section + ']', '', entry);
      continue;
    }
    let end = start + 1;
    while (end < lines.length && !lines[end].startsWith('[')) {
      end += 1;
    }
    while (end > start + 1 && lines[end - 1] === '') {
      end -= 1;
    }
    lines.splice(end, 0, entry);
  }
  fs.writeFileSync(file, lines.join('\n') + '\n');
}

function resPath(godotRoot, resourcePath) {
  if (!resourcePath.startsWith('res://')) {
    throw new Error('Expected a res:// path: ' + resourcePath);
  }
  return path.join(godotRoot, resourcePath.slice('res://'.length));
}

function parseStringArray(value) {
  return [...String(value).matchAll(/"((?:[^"\\]|\\.)*)"/g)].map(match => JSON.parse(`"${match[1]}"`));
}

function loadGodotProject(start) {
  const godotRoot = findGodotRoot(start);
  if (godotRoot == null) {
    return null;
  }
  const settings = {...SETTING_DEFAULTS, ...readProjectSettings(godotRoot)};
  return {
    godotRoot,
    jsRoot: resPath(godotRoot, settings['react_native/project/root']),
    bundlePath: resPath(godotRoot, settings['react_native/bundle/path']),
    entry: settings['react_native/dev_server/entry'],
    segments: parseStringArray(settings['react_native/bundle/segments']),
    devServer: {
      enabled: settings['react_native/dev_server/enabled'],
      host: settings['react_native/dev_server/host'],
      port: settings['react_native/dev_server/port'],
    },
  };
}

function ignoreNodeModules(jsRoot) {
  const nodeModules = path.join(jsRoot, 'node_modules');
  const marker = path.join(nodeModules, '.gdignore');
  if (fs.existsSync(nodeModules) && !fs.existsSync(marker)) {
    fs.writeFileSync(marker, '');
  }
}

module.exports = {
  addProjectSettings,
  findGodotRoot,
  ignoreNodeModules,
  loadGodotProject,
  readProjectSettings,
  resPath,
};
