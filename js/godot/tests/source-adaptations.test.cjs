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
