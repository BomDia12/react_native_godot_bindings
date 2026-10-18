const fs = require('fs');
const path = require('path');

module.exports = {
  presets: ['module:@react-native/babel-preset'],
  overrides: [
    {
      // The linked react-native-godot package implements React Native's
      // platform layer, so it imports React Native internals by design. An
      // installed copy lives under node_modules, where the preset never warns.
      test: fs.realpathSync(path.dirname(require.resolve('react-native-godot/package.json'))),
      presets: [['module:@react-native/babel-preset', {disableDeepImportWarnings: true}]],
    },
  ],
};
