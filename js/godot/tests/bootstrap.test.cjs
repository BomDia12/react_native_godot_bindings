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
