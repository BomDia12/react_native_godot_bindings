'use strict';

const path = require('path');
const {adapterFor} = require('./rn-overrides.cjs');

function createGodotResolver({reactNativeRoot, godotRoot}) {
  return (context, moduleName) => {
    const resolved = context.resolveRequest(context, moduleName, 'godot');
    if (resolved.type !== 'sourceFile') {
      return resolved;
    }
    const adapter = adapterFor(resolved.filePath, reactNativeRoot, godotRoot);
    return adapter == null ? resolved : {type: 'sourceFile', filePath: adapter};
  };
}

function isForeignPlatformModule(filePath) {
  const normalized = filePath.replaceAll(path.sep, '/');
  return /\.(android|ios)\.(js|jsx|ts|tsx)$/.test(normalized);
}

module.exports = {createGodotResolver, isForeignPlatformModule};
