'use strict';

const path = require('path');
const {createRequire} = require('module');
const {getDefaultConfig, mergeConfig} = require('@react-native/metro-config');
const {createGodotResolver} = require('./resolver.cjs');

function createGodotMetroConfig(projectRoot) {
  const resolvedProjectRoot = path.resolve(projectRoot);
  const projectRequire = createRequire(path.join(resolvedProjectRoot, 'package.json'));
  const reactNativeRoot = path.dirname(
    projectRequire.resolve('react-native/package.json'),
  );
  const packageRoot = __dirname;
  const defaults = getDefaultConfig(resolvedProjectRoot);
  return mergeConfig(defaults, {
    projectRoot: resolvedProjectRoot,
    watchFolders: [resolvedProjectRoot, packageRoot],
    resolver: {
      assetExts: Array.from(new Set([...(defaults.resolver.assetExts ?? []), 'woff', 'woff2'])),
      platforms: Array.from(
        new Set(['godot', ...(defaults.resolver.platforms ?? [])]),
      ),
      nodeModulesPaths: [path.join(resolvedProjectRoot, 'node_modules')],
      resolveRequest: createGodotResolver({projectRoot: resolvedProjectRoot, reactNativeRoot}),
    },
    serializer: {
      getModulesRunBeforeMainModule: () => [
        projectRequire.resolve('react-native/Libraries/Core/InitializeCore'),
        path.join(packageRoot, 'src/setup.js'),
      ],
    },
    transformer: {
      assetPlugins: [path.join(packageRoot, 'assets.cjs')],
    },
  });
}

module.exports = {createGodotMetroConfig};
