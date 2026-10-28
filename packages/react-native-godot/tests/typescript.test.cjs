'use strict';

const {test} = require('node:test');
const {execFileSync} = require('node:child_process');
const path = require('node:path');

test('declarations type-check from a consumer', () => {
  const tsc = require.resolve('typescript/bin/tsc');
  execFileSync(process.execPath, [tsc, '-p', path.join(__dirname, 'fixtures/typescript')], {stdio: 'inherit'});
});

test('the project template type-checks with the shipped tsconfig', () => {
  const tsc = require.resolve('typescript/bin/tsc');
  execFileSync(process.execPath, [tsc, '-p', path.join(__dirname, 'fixtures/template')], {stdio: 'inherit'});
});
