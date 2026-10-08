'use strict';
const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const {adaptSource, adaptations} = require('../source-adaptations.cjs');
const root = path.resolve(__dirname, '../../../samples/view-text/node_modules/react-native');
for (const relative of Object.keys(adaptations)) {
  test('pinned source adaptation: ' + relative, () => {
    const filename = path.join(root, relative);
    const source = fs.readFileSync(filename, 'utf8');
    const result = adaptSource(filename, source);
    assert.notEqual(result, source);
    assert.throws(() => adaptSource(filename, source + '\n'), /pinned RN 0.87.1/);
    assert.equal(adaptSource('/project/custom/' + relative, source), source);
    if (relative.endsWith('/TextInput.js')) {
      assert.match(result, /require\('react-native-godot\/TextInputHost'\)/);
      assert.match(result, /multiline=\{multiline\}/);
      assert.match(result, /useTextInputStateSynchronization/);
      assert.match(result, /setTextAndSelection/);
    }
    if (relative.endsWith('/TouchableOpacity.js')) {
      assert.match(result, /useNativeDriver: Platform.OS !== 'godot'/);
    }
    if (relative.endsWith('/ScrollView.js')) {
      assert.match(result, /Platform.OS === 'godot'[\s\S]*_scrollAnimatedValueAttachment = null;/);
      assert.match(result, /_scrollAnimatedValue.setValue\(e.nativeEvent.contentOffset.y\)/);
      assert.match(result, /AnimatedImplementation.attachNativeEvent/);
    }
  });
}


test('tagged emitter listeners retain separate root origins including unattributed handlers', () => {
  const {createRequire} = require('node:module');
  const vm = require('node:vm');
  const projectRequire = createRequire(path.join(root, '../package.json'));
  const relative = 'Libraries/vendor/emitter/EventEmitter.js';
  const source = adaptSource(path.join(root, relative), fs.readFileSync(path.join(root, relative), 'utf8'));
  const transformed = projectRequire('@babel/core').transformSync(source, {
    filename: path.join(root, relative),
    presets: [[projectRequire.resolve('@react-native/babel-preset'), {enableBabelRuntime: false}]],
    babelrc: false, configFile: false,
  }).code;
  let origin = null;
  const scheduler = {
    getOrigin: () => origin,
    withOrigin(value, callback) {const previous = origin; origin = value; try {return callback();} finally {origin = previous;}},
  };
  const context = {module: {exports: {}}, exports: {}, require: projectRequire, __godotScheduler: scheduler};
  context.exports = context.module.exports; context.global = context;
  vm.runInNewContext(transformed, context);
  const Emitter = context.module.exports.default;
  const emitter = new Emitter();
  const seen = [];
  const ambient = emitter.addListener('change', () => seen.push(origin));
  scheduler.withOrigin(11, () => emitter.addListener('change', () => seen.push(origin)));
  scheduler.withOrigin(21, () => emitter.addListener('change', () => seen.push(origin)));
  scheduler.withOrigin(31, () => emitter.emit('change'));
  assert.deepEqual(seen, [null, 11, 21]);
  assert.equal(origin, null);
  ambient.remove(); emitter.removeAllListeners('change');
  assert.equal(emitter.listenerCount('change'), 0);
});
