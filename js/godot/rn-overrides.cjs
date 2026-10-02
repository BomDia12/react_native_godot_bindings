'use strict';

const path = require('path');

const OVERRIDES = Object.freeze({
  'Libraries/Alert/RCTAlertManager': 'adapters/RCTAlertManager.godot.js',
  'Libraries/Components/AccessibilityInfo/legacySendAccessibilityEvent':
    'adapters/legacySendAccessibilityEvent.godot.js',
  'Libraries/Components/DrawerAndroid/DrawerLayoutAndroid':
    'adapters/UnsupportedComponent.godot.js',
  'Libraries/Components/ProgressBarAndroid/ProgressBarAndroid':
    'adapters/UnsupportedComponent.godot.js',
  'Libraries/Components/ToastAndroid/ToastAndroid':
    'adapters/UnsupportedAPI.godot.js',
  'Libraries/Image/Image': 'adapters/Image.godot.js',
  'Libraries/Image/resolveAssetSource': 'adapters/resolveAssetSource.godot.js',
  'Libraries/NativeComponent/BaseViewConfig':
    'adapters/BaseViewConfig.godot.js',
  'Libraries/Network/RCTNetworking': 'adapters/RCTNetworking.godot.js',
  'Libraries/StyleSheet/PlatformColorValueTypes':
    'adapters/PlatformColorValueTypes.godot.js',
  'Libraries/StyleSheet/processColor': 'adapters/processColor.godot.js',
  'Libraries/Utilities/BackHandler': 'adapters/BackHandler.godot.js',
  'Libraries/Utilities/Platform': 'adapters/Platform.godot.js',
});

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

function adapterFor(filePath, reactNativeRoot, godotRoot) {
  const key = canonicalRNPath(filePath, reactNativeRoot);
  const adapter = key == null ? null : OVERRIDES[key];
  return adapter == null ? null : path.join(godotRoot, adapter);
}

module.exports = {OVERRIDES, adapterFor, canonicalRNPath};
