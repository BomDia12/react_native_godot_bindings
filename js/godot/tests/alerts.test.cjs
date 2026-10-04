'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const {createRequire} = require('node:module');
const projectRequire = createRequire(path.resolve(__dirname, '../../../samples/view-text/package.json'));
const filename = path.resolve(__dirname, '../alerts.js');
const transformed = projectRequire('@babel/core').transformSync(fs.readFileSync(filename, 'utf8'), {
  filename, presets: [[projectRequire.resolve('@react-native/babel-preset'), {enableBabelRuntime: false}]],
  babelrc: false, configFile: false,
}).code;

test('Alert subscriptions follow root lifetime and stale cleanup cannot remove a replacement', () => {
  let created = 0;
  let removed = 0;
  const context = {module: {exports: {}},
    __godotNativeModules: {get: () => ({getOrigin: session => ({session}), onPresent() {created++; return {remove() {removed++;}};}})},
    __godotScheduler: {withoutOrigin: callback => callback()},
    require(name) {if (name === './modules') {return {};} return projectRequire(name);},
  };
  context.exports = context.module.exports; context.global = context;
  vm.runInNewContext(transformed, context);
  const register = context.module.exports.registerAlertRoot;
  const left = register(11, 'left', true);
  const right = register(21, 'right', true);
  assert.equal(created, 1);
  left(); assert.equal(removed, 0);
  right(); assert.equal(removed, 1);
  right(); assert.equal(removed, 1);
  const old = register(11, 'old', true);
  const current = register(11, 'current', true);
  old(); assert.equal(removed, 1);
  assert.equal(created, 2);
  current(); assert.equal(removed, 2);
});
