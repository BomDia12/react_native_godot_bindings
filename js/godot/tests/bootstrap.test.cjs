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

function freshTimers() {
  const context = vm.createContext({Date: {now: () => 0}});
  context.global = context;
  vm.runInContext(bootstrap, context);
  return context;
}

test('timer callbacks can cancel other callbacks in the current flush', () => {
  for (const [schedule, cancel] of [
    ['setTimeout', 'clearTimeout'],
    ['setInterval', 'clearInterval'],
    ['setImmediate', 'clearImmediate'],
    ['requestAnimationFrame', 'cancelAnimationFrame'],
  ]) {
    const timers = freshTimers();
    const called = [];
    let cancelled;
    timers.setTimeout(() => {
      called.push('first');
      timers[cancel](cancelled);
    }, 0);
    cancelled = timers[schedule](() => called.push('cancelled'), 0);
    timers.setTimeout(() => called.push('last'), 0);

    assert.doesNotThrow(() => timers.__godotFlushTimers());
    assert.deepEqual(called, ['first', 'last']);
    timers.__godotFlushTimers();
    assert.deepEqual(called, ['first', 'last']);
  }
});

test('timers created by callbacks wait until the next flush', () => {
  const timers = freshTimers();
  const called = [];
  timers.setTimeout(() => {
    called.push('first');
    timers.setTimeout(() => called.push('new'), 0);
  }, 0);

  timers.__godotFlushTimers();
  assert.deepEqual(called, ['first']);
  timers.__godotFlushTimers();
  assert.deepEqual(called, ['first', 'new']);
});
