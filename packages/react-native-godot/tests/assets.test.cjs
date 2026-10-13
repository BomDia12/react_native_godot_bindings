'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const {
  canonicalScale,
  godotAssetMetadata,
} = require('../assets.cjs');
const {pickScale} = require('../asset-scale.cjs');

test('asset metadata uses deterministic Godot URIs and scales', () => {
  const data = godotAssetMetadata({
    hash: 'abcdef',
    type: 'png',
    files: ['/tmp/a.png', '/tmp/a@2x.png', '/tmp/a@3x.png'],
    scales: [1, 2, 3],
  });
  assert.deepEqual(data.variants, [
    {scale: 1, uri: 'res://dist/assets/abcdef/1.png'},
    {scale: 2, uri: 'res://dist/assets/abcdef/2.png'},
    {scale: 3, uri: 'res://dist/assets/abcdef/3.png'},
  ]);
  assert.equal(canonicalScale(1.5), '1.5');
  assert.throws(() => canonicalScale(0), /Invalid/);
});

test('asset scale selection chooses the next scale or the largest variant', () => {
  const scales = [1, 2, 3];
  assert.equal(pickScale(scales, 1), 1);
  assert.equal(pickScale(scales, 1.5), 2);
  assert.equal(pickScale(scales, 2), 2);
  assert.equal(pickScale(scales, 4), 3);
});
