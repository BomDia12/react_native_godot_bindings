'use strict';

const path = require('path');
const {createRequire} = require('module');
const {createGodotResolver} = require('./resolver.cjs');

function createGodotMetroConfig(projectRoot) {
  const resolvedProjectRoot = path.resolve(projectRoot);
  const projectRequire = createRequire(path.join(resolvedProjectRoot, 'package.json'));
  const {getDefaultConfig, mergeConfig} = projectRequire(
    '@react-native/metro-config',
  );
  const reactNativeRoot = path.dirname(
    projectRequire.resolve('react-native/package.json'),
  );
  const godotRoot = __dirname;
  const defaults = getDefaultConfig(resolvedProjectRoot);
  return mergeConfig(defaults, {
    projectRoot: resolvedProjectRoot,
    watchFolders: [resolvedProjectRoot, godotRoot],
    resolver: {
      assetExts: Array.from(new Set([...(defaults.resolver.assetExts ?? []), 'woff', 'woff2'])),
      platforms: Array.from(
        new Set(['godot', ...(defaults.resolver.platforms ?? [])]),
      ),
      nodeModulesPaths: [path.join(resolvedProjectRoot, 'node_modules')],
      resolveRequest: createGodotResolver({reactNativeRoot, godotRoot}),
    },
    serializer: {
	  getPolyfills: options => [
		...(defaults.serializer.getPolyfills?.(options) ?? []),
		path.join(godotRoot, 'bootstrap.js'),
	  ],
      getModulesRunBeforeMainModule: () => [
        projectRequire.resolve('react-native/Libraries/Core/InitializeCore'),
        path.join(godotRoot, 'bootstrap-finalize.js'),
      ],
    },
    transformer: {
      assetPlugins: [path.join(godotRoot, 'assets.cjs')],
      babelTransformerPath: path.join(godotRoot, 'transformer.cjs'),
    },
  });
}

module.exports = {createGodotMetroConfig};
