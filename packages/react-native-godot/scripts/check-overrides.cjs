#!/usr/bin/env node
'use strict';

const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

const packageRoot = path.resolve(__dirname, '..');
const projectRoot = path.resolve(process.argv[2] ?? packageRoot);
const reactNativeRoot = path.dirname(require.resolve('react-native/package.json', {paths: [projectRoot]}));
const overrides = require(path.join(packageRoot, 'overrides.json'));

const failures = [];
for (const {file, baseFile, baseHash} of overrides) {
  if (!fs.existsSync(path.join(packageRoot, file))) {
    failures.push(`${file}: override file is missing`);
    continue;
  }
  const actual = crypto.createHash('sha256').update(fs.readFileSync(path.join(reactNativeRoot, baseFile))).digest('hex');
  if (actual !== baseHash) {
    failures.push(`${file}: react-native ${baseFile} changed (expected ${baseHash}, got ${actual}); re-derive the override from the new source`);
  }
}

if (failures.length > 0) {
  console.error(failures.join('\n'));
  process.exitCode = 1;
} else {
  console.log(`${overrides.length} tracked overrides match ${reactNativeRoot}`);
}
