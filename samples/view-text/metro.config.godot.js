const {getDefaultConfig, mergeConfig} = require('@react-native/metro-config');

// React Native's Metro config runs Libraries/Core/InitializeCore *before* the entry
// module (serializer.getModulesRunBeforeMainModule). So importing the preamble from
// godot.entry.js is too late: InitializeCore has already read RN$Bridgeless and
// friends. Put the preamble ahead of it in that same list instead.
const config = {
  serializer: {
    getModulesRunBeforeMainModule: () => [
      require.resolve('./godot.preamble.js'),
      require.resolve('react-native/Libraries/Core/InitializeCore'),
    ],
  },
};

module.exports = mergeConfig(getDefaultConfig(__dirname), config);

