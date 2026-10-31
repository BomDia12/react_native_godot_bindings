#!/usr/bin/env node
'use strict';

const path = require('path');
const {generateLibrary} = require('../cli/codegen.cjs');

const packageRoot = path.resolve(__dirname, '..');
const outputRoot = path.resolve(packageRoot, '../../modules/react_native_bindings/generated');
const reactNativeRoot = path.dirname(require.resolve('react-native/package.json', {paths: [packageRoot]}));

generateLibrary('FBReactNativeSpec', path.join(reactNativeRoot, 'src'), outputRoot);
generateLibrary('ReactNativeGodotSpec', path.join(packageRoot, 'specs'), outputRoot);
