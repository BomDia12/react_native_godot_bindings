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
      getPolyfills: options => [
        ...(defaults.serializer.getPolyfills?.(options) ?? []),
        path.join(packageRoot, 'src/bootstrap.js'),
      ],
      getModulesRunBeforeMainModule: () => [
        path.join(packageRoot, 'src/native-facades-install.js'),
        projectRequire.resolve('react-native/Libraries/Core/InitializeCore'),
        path.join(packageRoot, 'src/bootstrap-finalize.js'),
        path.join(packageRoot, 'src/setup.js'),
      ],
    },
    transformer: {
      assetPlugins: [path.join(packageRoot, 'assets.cjs')],
      babelTransformerPath: path.join(packageRoot, 'transformer.cjs'),
    },
  });
}

module.exports = {createGodotMetroConfig};
