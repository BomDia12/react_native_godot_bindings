'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const {createRequire} = require('node:module');
const path = require('node:path');
const vm = require('node:vm');
const {isForeignPlatformModule} = require('../resolver.cjs');

const packageRoot = path.resolve(__dirname, '..');
const Metro = createRequire(path.join(packageRoot, 'package.json'))('metro');
const config = require('../metro.cjs').createGodotMetroConfig(packageRoot);

for (const dev of [false, true]) {
  test(
    'real Metro ' + (dev ? 'development' : 'production') + ' build selects Godot extensions',
    {timeout: 120000},
    async () => {
      const result = await Metro.runBuild(config, {
        dev,
        entry: 'tests/fixtures/resolver/entry.js',
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
      // The bridgeless globals that upstream's ReactInstance installs before a bundle runs.
      const context = {
        console,
        RN$Bridgeless: true,
        RN$registerCallableModule() {},
        __turboModuleProxy: () => null,
        nativeModuleProxy: {},
        setTimeout: () => 0,
        clearTimeout() {},
        setInterval: () => 0,
        clearInterval() {},
        queueMicrotask,
      };
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
