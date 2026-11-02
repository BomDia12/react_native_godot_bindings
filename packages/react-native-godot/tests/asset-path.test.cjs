'use strict';

const assert = require('node:assert');
const {test} = require('node:test');
const {metroAssetResourcePath} = require('../asset-path.cjs');

test('a Metro asset maps to its file under the project', () => {
  const asset = {httpServerLocation: '/assets/sounds', name: 'click', type: 'wav'};
  assert.strictEqual(metroAssetResourcePath(asset, 'res://react_native/'), 'res://react_native/sounds/click.wav');
  assert.strictEqual(metroAssetResourcePath(asset, 'res://'), 'res://sounds/click.wav');
});

test('an asset outside the project has no resource path', () => {
  const asset = {httpServerLocation: '/assets/?unstable_path=..%2Fshared', name: 'click', type: 'wav'};
  assert.strictEqual(metroAssetResourcePath(asset, 'res://'), null);
  assert.strictEqual(metroAssetResourcePath(undefined, 'res://'), null);
});
