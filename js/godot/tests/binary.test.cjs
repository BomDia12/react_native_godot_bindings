'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const {createRequire} = require('node:module');
const projectRequire = createRequire(path.resolve(__dirname, '../../../samples/view-text/package.json'));
const filename = path.resolve(__dirname, '../binary.js');
const transformed = projectRequire('@babel/core').transformSync(fs.readFileSync(filename, 'utf8'), {
  filename, presets: [[projectRequire.resolve('@react-native/babel-preset'), {enableBabelRuntime: false}]],
  babelrc: false, configFile: false,
}).code;
const context = {module: {exports: {}}, require: projectRequire, Uint8Array};
context.exports = context.module.exports;
context.global = context;
vm.runInNewContext(transformed, context);
const {utf8Encode, utf8Decode, createFromParts} = context.module.exports;

test('UTF-8 matches TextEncoder for paired and unpaired UTF-16, NUL, and boundaries', () => {
  for (const text of ['', '\0', 'a\ud800b\udc00', '\ud800', '\udfff', '\ud800\ud800', '\udc00\ud800', '\ud800\udc00', '\udbff\udfff', '\u007f\u0080\u07ff\u0800\ud7ff\ue000\uffff😀']) {
    const expected = new TextEncoder().encode(text);
    assert.deepEqual(utf8Encode(text), expected);
    assert.equal(utf8Decode(expected), new TextDecoder().decode(expected));
  }
});

test('Blob string parts upload replacement bytes and respect chunk boundaries', () => {
  const chunks = [];
  let size;
  context.__godotNativeModules = {get() {return {
    begin(id, total) {assert.equal(id, 'fixture'); size = total;},
    append(id, offset, bytes) {assert.equal(id, 'fixture'); assert.ok(bytes.length <= 1024 * 1024); chunks.push({offset, bytes});},
    finish(id) {assert.equal(id, 'fixture');},
    release() {assert.fail('valid UTF-16 must not release an unfinished blob');},
  };}};
  const text = 'x'.repeat(1024 * 1024 - 1) + '\ud800\0';
  createFromParts([{type: 'string', data: text}], 'fixture');
  assert.equal(chunks.length, 2);
  const uploaded = new Uint8Array(size);
  for (const {offset, bytes} of chunks) {uploaded.set(bytes, offset);}
  assert.deepEqual(uploaded, new TextEncoder().encode(text));
});
