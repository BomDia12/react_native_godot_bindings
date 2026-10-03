'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const {createRequire} = require('node:module');
const path = require('node:path');
const vm = require('node:vm');
const {isForeignPlatformModule} = require('../resolver.cjs');

const sampleRoot = path.resolve(__dirname, '../../../samples/view-text');
const sampleRequire = createRequire(path.join(sampleRoot, 'package.json'));
const Metro = sampleRequire('metro');
const config = require(path.join(sampleRoot, 'metro.config.godot.js'));

for (const dev of [false, true]) {
  test(
    'real Metro ' + (dev ? 'development' : 'production') + ' build selects Godot extensions',
    {timeout: 120000},
    async () => {
      const result = await Metro.runBuild(config, {
        dev,
        entry: 'godot-fixtures/resolver/entry.js',
        minify: false,
        platform: 'godot',
        sourceMap: true,
      });
      for (const marker of [
        'godot-js',
        'godot-jsx',
        'godot-ts',
        'godot-tsx',
        'native-fallback',
        'generic-fallback',
      ]) {
        assert.match(result.code, new RegExp(marker));
      }
      assert.doesNotMatch(result.code, /wrong-native-js|wrong-generic-js/);
      const context = {console, __godotScheduler: Object.fromEntries(
        ['setTimeout', 'setInterval', 'clearTimeout', 'clearInterval', 'requestAnimationFrame', 'cancelAnimationFrame', 'now', 'requestIdleCallback', 'cancelIdleCallback'].map(name => [name, () => 0]),
      )};
      context.globalThis = context;
      vm.runInNewContext(result.code, context);
      assert.equal(
        context.__godotResolverFixture.pointerEnterSkipsBubbling,
        true,
      );
      assert.equal(
        context.__godotResolverFixture.pointerLeaveSkipsBubbling,
        true,
      );
    },
  );
}

test('foreign platform module detection is extension-specific', () => {
  assert.equal(isForeignPlatformModule('/x/Thing.android.js'), true);
  assert.equal(isForeignPlatformModule('/x/Thing.ios.tsx'), true);
  assert.equal(isForeignPlatformModule('/x/AndroidNamedGeneric.js'), false);
  assert.equal(isForeignPlatformModule('/x/Thing.native.js'), false);
});
