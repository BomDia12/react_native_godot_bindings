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

function executeResponse(response, responseType, throwingEvent = null) {
  const events = [];
  const released = [];
  class Emitter {
    emit(name, value) {
      events.push([name, value]);
      if (name === throwingEvent) {throw new Error('listener failed');}
    }
  }
  const context = {module: {exports: {}}, queueMicrotask,
    __godotNativeModules: {get() {return {
      __godotStartAsync() {return {requestId: 'request', promise: Promise.resolve(response)};},
    };}},
    require(name) {
      if (name.endsWith('/EventEmitter')) {return Emitter;}
      if (name.endsWith('/convertRequestBody')) {return value => value;}
      if (name === '../binary') {return {
        binaryService: () => ({release: id => released.push(id)}),
        readBytes: () => new Uint8Array([65]), utf8Decode: () => 'A', readBase64: () => 'QQ==',
      };}
      return projectRequire(name);
    },
  };
  context.exports = context.module.exports; context.global = context;
  vm.runInNewContext(transformed, context);
  context.module.exports.default.sendRequest('GET', '', response.url, {}, null, responseType, false, 0, () => {}, false);
  return new Promise(resolve => setImmediate(() => resolve({events, released})));
}

const nativeResponse = () => ({status: 200, headers: [], url: 'https://fixture.test/',
  body: {blobId: 'response', offset: 0, size: 1, type: ''}});

test('response backing is released once when metadata or data listeners throw', async () => {
  for (const responseType of ['text', 'base64', 'blob']) {
    for (const throwingEvent of ['didReceiveNetworkResponse', 'didReceiveNetworkData']) {
      const {events, released} = await executeResponse(nativeResponse(), responseType, throwingEvent);
      assert.deepEqual(released, ['response']);
      assert.equal(events.at(-1)[0], 'didCompleteNetworkResponse');
      assert.equal(events.at(-1)[1][1], 'listener failed');
      assert.equal(events.filter(([name]) => name === 'didCompleteNetworkResponse').length, 1);
    }
  }
});

test('successful Blob handoff preserves backing even if completion listeners throw', async () => {
  for (const responseType of ['text', 'base64', 'blob']) {
    const {events, released} = await executeResponse(nativeResponse(), responseType, 'didCompleteNetworkResponse');
    assert.deepEqual(released, responseType === 'blob' ? [] : ['response']);
    assert.equal(events.filter(([name]) => name === 'didCompleteNetworkResponse').length, 1);
    assert.equal(events[1][0], 'didReceiveNetworkData');
  }
});

test('prototype-colliding response header names and duplicates retain exact values', async () => {
  const response = nativeResponse();
  response.headers = [['constructor', 'ctor'], ['__proto__', 'first'], ['toString', 'text'],
    ['hasOwnProperty', 'own'], ['__proto__', 'second'], ['constructor', 'next']];
  const {events, released} = await executeResponse(response, 'text');
  const headers = events[0][1][2];
  assert.equal(Object.getPrototypeOf(headers), null);
  assert.equal(headers.constructor, 'ctor, next');
  assert.equal(headers.__proto__, 'first, second');
  assert.equal(headers.toString, 'text');
  assert.equal(headers.hasOwnProperty, 'own');
  assert.deepEqual(Object.keys(headers), ['constructor', '__proto__', 'toString', 'hasOwnProperty']);
  assert.deepEqual(released, ['response']);
});

test('borrowed upload Blobs are pinned before enqueue and unpinned once on every terminal path', async () => {
  for (const mode of ['success', 'reject', 'abort', 'enqueue-failure', 'conversion-failure', 'callback-failure']) {
    for (const multipart of [false, true]) {
      const calls = [];
      const events = [];
      let resolve, reject;
      const promise = new Promise((yes, no) => {resolve = yes; reject = no;});
      const data = {blobId: 'borrowed', offset: 1, size: 3};
      const binary = {pin(value) {calls.push(['pin', value.blobId]);}, unpin(id) {calls.push(['unpin', id]);}, release(id) {calls.push(['release', id]);}};
      const context = {module: {exports: {}}, queueMicrotask,
        __godotNativeModules: {
          get() {return {__godotStartAsync(method, [request]) {
            calls.push(['enqueue']);
            assert.deepEqual(calls.slice(0, 2), [['pin', 'borrowed'], ['enqueue']]);
            const upload = multipart ? request.formData[0].blob : request.body;
            assert.equal(upload.offset, 1); assert.equal(upload.size, 3);
            if (mode === 'enqueue-failure') {throw new Error(mode);}
            return {requestId: 'native-request', promise};
          }};},
          cancel(id) {assert.equal(id, 'native-request'); calls.push(['cancel']); reject(new Error(mode));},
        },
        require(name) {
          if (name.endsWith('/EventEmitter')) {return class {emit(name, value) {events.push([name, value]);}};}
          if (name.endsWith('/convertRequestBody')) {return value => value;}
          if (name === '../binary') {return {
            binaryService: () => binary,
            descriptor(value) {if (!value) {throw new Error('conversion-failure');} return {...value};},
            readBytes: () => new Uint8Array([65]), utf8Decode: () => 'A',
          };}
          return projectRequire(name);
        },
      };
      context.exports = context.module.exports; context.global = context;
      vm.runInNewContext(transformed, context);
      const networking = context.module.exports.default;
      const body = multipart ? {formData: [{blob: data}, {data}, ...(mode === 'conversion-failure' ? [{_data: {blobId: 'invalid'}}] : [])]} : {blob: data};
      if (mode === 'conversion-failure') {
        binary.pin = value => {if (value.blobId === 'invalid') {throw new Error(mode);} calls.push(['pin', value.blobId]);};
        if (!multipart) {binary.pin = () => {throw new Error(mode);};}
      }
      let id;
      networking.sendRequest('POST', '', 'https://fixture.test/', {}, body, 'text', false, 0, value => {
        id = value; if (mode === 'callback-failure') {throw new Error(mode);}
      }, false);
      if (mode === 'success') {resolve(nativeResponse());}
      else if (mode === 'reject') {reject(new Error(mode));}
      else if (mode === 'abort') {networking.abortRequest(id);}
      await new Promise(done => setImmediate(done));
      const expectedPins = mode === 'conversion-failure' && !multipart ? 0 : 1;
      assert.equal(calls.filter(([name]) => name === 'pin').length, expectedPins);
      assert.equal(calls.filter(([name]) => name === 'unpin').length, expectedPins);
      assert.equal(calls.filter(([name, id]) => name === 'release' && id === 'borrowed').length, 0);
      assert.equal(events.filter(([name]) => name === 'didCompleteNetworkResponse').length, 1);
      assert.equal(events.at(-1)[1][1], mode === 'success' ? null : mode);
      assert.equal(calls.filter(([name]) => name === 'cancel').length, ['abort', 'callback-failure'].includes(mode) ? 1 : 0);
    }
  }
});
