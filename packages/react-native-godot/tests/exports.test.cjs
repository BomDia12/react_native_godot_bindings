'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

const packageRoot = path.resolve(__dirname, '..');
const compatibilityRoot = path.resolve(packageRoot, '../../documentation/compatibility');
const matrices = ['components.md', 'apis.md'];

function reactNativeExports() {
  const reactNativeRoot = path.dirname(require.resolve('react-native/package.json', {paths: [packageRoot]}));
  const indexPath = path.join(reactNativeRoot, 'index.js');
  const source = fs.readFileSync(indexPath, 'utf8');
  const start = source.indexOf('module.exports = {');
  const end = source.indexOf('\n}', start);
  assert.ok(start >= 0 && end > start, 'react-native/index.js has no module.exports object literal');
  const literal = source.slice(start, end);
  const names = [...literal.matchAll(/^ {2}(?:get )?(\w+)(?:<[^>]*>)?\(/gm)].map(match => match[1]);
  for (const match of source.matchAll(/defineProperty\(module\.exports, '(\w+)'/g)) {
    names.push(match[1]);
  }
  return names;
}

function cells(line) {
  return line.split('|').slice(1, -1).map(cell => cell.trim());
}

// A Surface cell takes part only when it is exactly one backticked identifier,
// so rows such as `name` (global) or package subpaths are not export claims.
function exportName(surface) {
  return /^`(\w+)`$/.exec(surface)?.[1];
}

function matrixEntries() {
  const entries = [];
  for (const file of matrices) {
    let outOfScope = false;
    for (const line of fs.readFileSync(path.join(compatibilityRoot, file), 'utf8').split('\n')) {
      if (line.startsWith('## ')) {
        outOfScope = line === '## Out of scope';
      }
      if (!line.startsWith('|') || line.startsWith('|---') || line.startsWith('| ID |') || line.startsWith('| Surface |')) {
        continue;
      }
      const name = exportName(cells(line)[outOfScope ? 0 : 1]);
      if (name) {
        entries.push({name, where: `${file} ${outOfScope ? 'out of scope' : 'row'}`});
      }
    }
  }
  return entries;
}

test('every react-native export has exactly one matrix row or out-of-scope entry', () => {
  const exported = reactNativeExports();
  assert.ok(exported.length > 90, `only ${exported.length} exports were found in react-native/index.js`);
  const entries = matrixEntries();
  const problems = [];
  for (const name of exported) {
    const matching = entries.filter(entry => entry.name === name);
    if (matching.length !== 1) {
      problems.push(`${name}: ${matching.length ? matching.map(entry => entry.where).join(', ') : 'no entry'}`);
    }
  }
  for (const entry of entries) {
    if (!exported.includes(entry.name)) {
      problems.push(`${entry.name}: ${entry.where} names something react-native does not export`);
    }
  }
  assert.deepEqual(problems, []);
});
