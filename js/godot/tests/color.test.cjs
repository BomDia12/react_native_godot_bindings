'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const {createRequire} = require('node:module');
const path = require('node:path');
const {processGodotColor} = require('../color.cjs');

const sampleRoot = path.resolve(__dirname, '../../../samples/view-text');
const sampleRequire = createRequire(path.join(sampleRoot, 'package.json'));
const normalizeColorModule = sampleRequire('@react-native/normalize-colors');
const normalizeColor = normalizeColorModule.default ?? normalizeColorModule;

test('normalizes public React Native colors to explicit RGBA', () => {
  const cases = [
    ['red', [1, 0, 0, 1]],
    ['#123456', [0x12 / 255, 0x34 / 255, 0x56 / 255, 1]],
    [
      'rgba(10, 20, 30, 0.5)',
      [10 / 255, 20 / 255, 30 / 255, 0x80 / 255],
    ],
    ['hsl(120, 100%, 50%)', [0, 1, 0, 1]],
    ['transparent', [0, 0, 0, 0]],
    [0xff000080, [1, 0, 0, 0x80 / 255]],
  ];
  for (const [input, expected] of cases) {
    const result = processGodotColor(input, normalizeColor);
    assert.equal(result.$godot, 'Color');
    expected.forEach((channel, index) => {
      assert.ok(
        Math.abs([result.r, result.g, result.b, result.a][index] - channel) <
          1e-6,
      );
    });
  }
  assert.equal(processGodotColor(null, normalizeColor), null);
  assert.equal(processGodotColor(undefined, normalizeColor), undefined);
});

test('accepts valid wrappers and rejects malformed or dynamic colors', () => {
  const color = {$godot: 'Color', r: 0, g: 0.5, b: 1, a: 1};
  assert.equal(processGodotColor(color, normalizeColor), color);
  assert.throws(
    () =>
      processGodotColor(
        {$godot: 'Color', r: 2, g: 0, b: 0, a: 1},
        normalizeColor,
      ),
    /0-1/,
  );
  assert.throws(
    () =>
      processGodotColor(
        {dynamic: {light: 'white', dark: 'black'}},
        value => value,
      ),
    error => error.code === 'E_UNSUPPORTED',
  );
});
