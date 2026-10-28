'use strict';

const fs = require('fs');
const reactNativePreset = require('@react-native/babel-preset');

// This package implements React Native's platform layer, so it imports React
// Native internals by design. The preset only warns about that outside
// node_modules, which is where a linked copy of this package lives.
module.exports = (_api, options) => ({
  presets: [[reactNativePreset, options]],
  overrides: [
    {
      test: fs.realpathSync(__dirname),
      presets: [[reactNativePreset, {...options, disableDeepImportWarnings: true}]],
    },
  ],
});
