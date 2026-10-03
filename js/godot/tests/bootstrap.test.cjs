'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const bootstrap = fs.readFileSync(
  path.join(__dirname, '../bootstrap.js'),
  'utf8',
);

test('bootstrap requires native scheduling and installs native timer functions', () => {
  const absent = vm.createContext({});
  absent.global = absent;
  assert.throws(() => vm.runInContext(bootstrap, absent), /native scheduler/);
  const names = ['setTimeout', 'setInterval', 'clearTimeout', 'clearInterval',
    'requestAnimationFrame', 'cancelAnimationFrame', 'now'];
  const scheduler = Object.fromEntries(names.map(name => [name, () => name]));
  const context = vm.createContext({__godotScheduler: scheduler, require: () => ({nativeFacade: () => null})});
  context.global = context;
  vm.runInContext(bootstrap, context);
  for (const name of names.filter(name => name !== 'now')) {
    assert.equal(context[name], scheduler[name]);
  }
  assert.equal(context.nativePerformanceNow, scheduler.now);
  assert.equal(context.__turboModuleProxy('NativeIdleCallbacksCxx'), scheduler);
});

test('Keyboard-only consumers instantiate native services before subscribing', () => {
  const {createRequire} = require('node:module');
  const projectRequire = createRequire(path.resolve(__dirname, '../../../samples/view-text/package.json'));
  const filename = path.resolve(__dirname, '../native-facades.js');
  const transformed = projectRequire('@babel/core').transformSync(fs.readFileSync(filename, 'utf8'), {
    filename,
    presets: [[projectRequire.resolve('@react-native/babel-preset'), {enableBabelRuntime: false}]],
    babelrc: false, configFile: false,
  }).code;
  let initialized = false;
  let subscription;
  const events = [];
  const services = {
    getState() {initialized = true; return {};},
    onEvent(callback) {assert.equal(initialized, true); subscription = callback; return {remove() {}};},
  };
  const context = {module: {exports: {}}, exports: {},
    __godotNativeModules: {get(name) {assert.equal(name, 'GodotServices'); return services;}},
    __godotScheduler: {withoutOrigin(callback) {return callback();}},
    require(name) {
      if (name === './websocket-facade' || name === './binary') {return {};}
      if (name === 'react-native/Libraries/EventEmitter/RCTDeviceEventEmitter') {
        return {default: {emit(name, payload) {events.push([name, payload]);}}};
      }
      return projectRequire(name);
    },
  };
  context.exports = context.module.exports; context.global = context;
  vm.runInNewContext(transformed, context);
  const observer = context.module.exports.nativeFacade('KeyboardObserver');
  assert.equal(typeof observer.addListener, 'function');
  subscription({name: 'keyboardDidShow', payload: {height: 100}});
  assert.deepEqual(events, [['keyboardDidShow', {height: 100}]]);
});
