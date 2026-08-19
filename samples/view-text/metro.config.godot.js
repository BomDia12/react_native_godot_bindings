const {getDefaultConfig, mergeConfig} = require('@react-native/metro-config');

// InitializeCore reads these globals before the entry module runs.
const config = {
  serializer: {
    getModulesRunBeforeMainModule: () => [
      require.resolve('./godot.preamble.js'),
      require.resolve('react-native/Libraries/Core/InitializeCore'),
    ],
  },
};

module.exports = mergeConfig(getDefaultConfig(__dirname), config);
