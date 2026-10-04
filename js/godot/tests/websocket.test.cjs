'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const {createRequire} = require('node:module');
const projectRequire = createRequire(path.resolve(__dirname, '../../../samples/view-text/package.json'));
const filename = path.resolve(__dirname, '../websocket-facade.js');
const transformed = projectRequire('@babel/core').transformSync(fs.readFileSync(filename, 'utf8'), {
  filename, presets: [[projectRequire.resolve('@react-native/babel-preset'), {enableBabelRuntime: false}]],
  babelrc: false, configFile: false,
}).code;

test('WebSocket Blob begin/write/finish failures release partial storage and terminate once', () => {
  for (const phase of ['begin', 'append', 'finish']) {
    let receive;
    const released = [];
    const closed = [];
    const events = [];
    const service = Object.fromEntries(['begin', 'append', 'finish'].map(name => [name, () => {
      if (name === phase) {throw new Error(phase + ' failed');}
    }]));
    service.release = id => released.push(id);
    const socket = {onEvent(callback) {receive = callback;}, connect() {}, close(...args) {closed.push(args);}};
    const context = {module: {exports: {}},
      __godotScheduler: {withoutOrigin: callback => callback()},
      __godotNativeModules: {get: name => name === 'GodotWebSocket' ? socket : service},
      require(name) {
        if (name.endsWith('/RCTDeviceEventEmitter')) {return {emit: (name, payload) => events.push([name, payload])};}
        if (name === './binary') {return {CHUNK_BYTES: 1, utf8Decode: () => 'text'};}
        return projectRequire(name);
      },
    };
    context.exports = context.module.exports; context.global = context;
    vm.runInNewContext(transformed, context);
    const {websocketFacade, socketBlobMode} = context.module.exports;
    websocketFacade.connect('ws://fixture.test', [], {}, 1); socketBlobMode(1, true);
    const packet = {id: 1, text: false, bytes: new Uint8Array([65, 0, 66])};
    receive({name: 'websocketMessage', payload: packet});
    assert.equal(events.length, 1);
    assert.equal(events[0][0], 'websocketFailed');
    assert.equal(events[0][1].id, 1);
    assert.equal(events[0][1].message, phase + ' failed');
    assert.deepEqual(closed, [[1, 1000, 'Blob conversion failed']]);
    assert.deepEqual(released, phase === 'begin' ? [] : ['socket-1-1']);
    receive({name: 'websocketMessage', payload: packet});
    receive({name: 'websocketMessage', payload: {...packet, text: true}});
    receive({name: 'websocketClosed', payload: {id: 1, code: 1000}});
    assert.equal(events.length, 1);
    receive({name: 'websocketMessage', payload: {...packet, id: 2, text: true}});
    assert.equal(events.length, 2);
    assert.equal(events[1][1].id, 2);
    assert.equal(events[1][1].data, 'text');
  }
});
