'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const {createRequire} = require('node:module');
const projectRequire = createRequire(path.resolve(__dirname, '../../../samples/view-text/package.json'));
const filename = path.resolve(__dirname, '../adapters/RCTNetworking.godot.js');
const transformed = projectRequire('@babel/core').transformSync(fs.readFileSync(filename, 'utf8'), {
  filename, presets: [[projectRequire.resolve('@react-native/babel-preset'), {enableBabelRuntime: false}]],
  babelrc: false, configFile: false,
}).code;

test('Blob responses carry case-insensitive Content-Type metadata without mutating native descriptors', async () => {
  for (const header of ['Content-Type', 'cOnTeNt-TyPe', 'CONTENT-TYPE', null]) {
    const events = [];
    const body = {blobId: 'response', offset: 0, size: 3, type: ''};
    const response = {status: 200, headers: header ? [[header, 'image/png']] : [], url: 'https://fixture.test/', body};
    class Emitter { emit(name, value) {events.push([name, value]);} }
    const context = {module: {exports: {}}, queueMicrotask,
      __godotNativeModules: {get(name) {assert.equal(name, 'GodotHTTP'); return {
        __godotStartAsync(method) {assert.equal(method, 'send'); return {requestId: 'request', promise: Promise.resolve(response)};},
      };}},
      require(name) {
        if (name.endsWith('/EventEmitter')) {return Emitter;}
        if (name.endsWith('/convertRequestBody')) {return value => value;}
        if (name === '../binary') {return {binaryService() {assert.fail('Blob response backing must remain owned by the recipient');}};}
        return projectRequire(name);
      },
    };
    context.exports = context.module.exports; context.global = context;
    vm.runInNewContext(transformed, context);
    const networking = context.module.exports.default;
    let requestId;
    networking.sendRequest('GET', '', response.url, {}, null, 'blob', false, 0, id => {requestId = id;}, false);
    await new Promise(resolve => setImmediate(resolve));
    assert.deepEqual(events.map(([name]) => name), ['didReceiveNetworkResponse', 'didReceiveNetworkData', 'didCompleteNetworkResponse']);
    const data = events[1][1];
    assert.equal(data[0], requestId);
    assert.equal(data[1].type, header ? 'image/png' : '');
    assert.equal(data[1].blobId, body.blobId);
    assert.equal(body.type, '');
    assert.equal(events[2][1][1], null);
  }
});
