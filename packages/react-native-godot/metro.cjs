'use strict';

const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const {createRequire} = require('module');
const {getDefaultConfig, mergeConfig} = require('@react-native/metro-config');
const {createGodotResolver} = require('./resolver.cjs');

// Dependencies installed from local paths are symlinks to directories outside
// the project, which Metro only reads through its watch folders.
function linkedDependencyRoots(projectRoot) {
  const manifest = JSON.parse(fs.readFileSync(path.join(projectRoot, 'package.json'), 'utf8'));
  return Object.keys(manifest.dependencies ?? {})
    .map(name => path.join(projectRoot, 'node_modules', name))
    .filter(installed => fs.existsSync(installed))
    .map(installed => fs.realpathSync(installed))
    .filter(root => path.relative(projectRoot, root).startsWith('..'));
}

// Module IDs hash the project-relative path, so a segment built on its own
// refers to the modules of the bundle it extends by the same IDs.
function stableModuleIds(projectRoot) {
  return () => modulePath => {
    const relative = path.relative(projectRoot, modulePath).split(path.sep).join('/');
    return crypto.createHash('sha1').update(relative).digest().readUInt32BE(0) & 0x7fffffff;
  };
}

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
    watchFolders: Array.from(new Set([resolvedProjectRoot, packageRoot, ...linkedDependencyRoots(resolvedProjectRoot)])),
    resolver: {
      assetExts: Array.from(new Set([...(defaults.resolver.assetExts ?? []), 'woff', 'woff2'])),
      platforms: Array.from(
        new Set(['godot', ...(defaults.resolver.platforms ?? [])]),
      ),
      nodeModulesPaths: [path.join(resolvedProjectRoot, 'node_modules')],
      resolveRequest: createGodotResolver({projectRoot: resolvedProjectRoot, reactNativeRoot}),
    },
    serializer: {
      createModuleIdFactory: stableModuleIds(resolvedProjectRoot),
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
