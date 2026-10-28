'use strict';

const cli = require('./cli/index.cjs');

module.exports = {
  commands: cli.commands,
  platforms: {
    godot: {
      projectConfig: cli.projectConfig,
      dependencyConfig: cli.dependencyConfig,
      npmPackageName: 'react-native-godot',
    },
  },
};
