'use strict';

const path = require('path');
const {adapterFor} = require('./rn-overrides.cjs');

function createGodotResolver({reactNativeRoot, godotRoot}) {
  return (context, moduleName) => {
    if (moduleName === 'react-native-godot/UpstreamURL') { return {type: 'sourceFile', filePath: path.join(reactNativeRoot, 'Libraries/Blob/URL.js')}; }
    if (moduleName === 'react-native-godot/app-registry') { return {type: 'sourceFile', filePath: path.join(godotRoot, 'app-registry.js')}; }
    if (moduleName === 'react-native-godot/binary') { return {type: 'sourceFile', filePath: path.join(godotRoot, 'binary.js')}; }
    if (moduleName === 'react-native-godot/scene') { return {type: 'sourceFile', filePath: path.join(godotRoot, 'scene.js')}; }
    if (moduleName === 'react-native-godot/GodotWindow') { return {type: 'sourceFile', filePath: path.join(godotRoot, 'components/GodotWindow.js')}; }
    if (moduleName === 'react-native-godot/fonts') { return {type: 'sourceFile', filePath: path.join(godotRoot, 'fonts.js')}; }
    if (moduleName === 'react-native-godot/TextInputHost') { return {type: 'sourceFile', filePath: path.join(godotRoot, 'adapters/TextInputHost.godot.js')}; }
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
