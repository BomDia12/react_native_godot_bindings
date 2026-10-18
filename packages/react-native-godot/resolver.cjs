'use strict';

const fs = require('fs');
const path = require('path');

const packageRoot = fs.realpathSync(__dirname);

function canonicalRNPath(filePath, reactNativeRoot) {
  const relative = path.relative(reactNativeRoot, filePath);
  if (relative.startsWith('..') || path.isAbsolute(relative)) {
    return null;
  }
  return relative
    .replaceAll(path.sep, '/')
    .replace(/\.(android|ios|native|godot)?\.(js|jsx|ts|tsx)$/, '')
    .replace(/\.(js|jsx|ts|tsx)$/, '');
}

function godotOverrideFor(filePath, reactNativeRoot) {
  const key = canonicalRNPath(filePath, reactNativeRoot);
  if (key == null || !(key.startsWith('Libraries/') || key.startsWith('src/private/'))) {
    return null;
  }
  const override = path.join(packageRoot, key + '.godot.js');
  return fs.existsSync(override) ? override : null;
}

function isInsidePackage(filePath) {
  const relative = path.relative(packageRoot, filePath);
  return !relative.startsWith('..') && !path.isAbsolute(relative);
}

// Bare imports from this package resolve as if the application made them, so the
// package always shares the application's single react-native installation.
function createGodotResolver({projectRoot, reactNativeRoot}) {
  const projectOrigin = path.join(projectRoot, 'package.json');
  return (context, moduleName) => {
    const bareFromPackage = !moduleName.startsWith('.') && isInsidePackage(context.originModulePath);
    const origin = bareFromPackage ? {...context, originModulePath: projectOrigin} : context;
    let resolved;
    try {
      resolved = origin.resolveRequest(origin, moduleName, 'godot');
    } catch (error) {
      // Modules that React Native ships only as Android and iOS files have no
      // generic file to resolve, so only their Godot platform file can match.
      const override = moduleName.startsWith('.') ? godotOverrideFor(path.resolve(path.dirname(context.originModulePath), moduleName), reactNativeRoot) : null;
      if (override == null) {
        throw error;
      }
      return {type: 'sourceFile', filePath: override};
    }
    if (resolved.type !== 'sourceFile') {
      return resolved;
    }
    const override = godotOverrideFor(resolved.filePath, reactNativeRoot);
    return override == null || override === context.originModulePath ? resolved : {type: 'sourceFile', filePath: override};
  };
}

function isForeignPlatformModule(filePath) {
  const normalized = filePath.replaceAll(path.sep, '/');
  return /\.(android|ios)\.(js|jsx|ts|tsx)$/.test(normalized);
}

module.exports = {canonicalRNPath, createGodotResolver, godotOverrideFor, isForeignPlatformModule};
