#!/usr/bin/env node
'use strict';

const fs = require('fs');
const path = require('path');
const RNCodegen = require('@react-native/codegen/lib/generators/RNCodegen');
const {combineSchemasInFileList} = require('@react-native/codegen/lib/cli/combine/combine-js-to-schema');

const packageRoot = path.resolve(__dirname, '..');
const outputRoot = path.resolve(packageRoot, '../../modules/react_native_bindings/generated');
const reactNativeRoot = path.dirname(require.resolve('react-native/package.json', {paths: [packageRoot]}));

const component = name => require(`@react-native/codegen/lib/generators/components/${name}`).generate;
// Upstream keys its output folders by generator name; these keys reuse the
// componentsAndroid and modulesCxx folders with only the C++ generators.
const generators = {
  componentsAndroid: [
    component('GenerateComponentDescriptorH'),
    component('GenerateComponentDescriptorCpp'),
    component('GenerateEventEmitterH'),
    component('GenerateEventEmitterCpp'),
    component('GeneratePropsH'),
    component('GeneratePropsCpp'),
    component('GenerateStateH'),
    component('GenerateStateCpp'),
    component('GenerateShadowNodeH'),
    component('GenerateShadowNodeCpp'),
  ],
  modulesCxx: [require('@react-native/codegen/lib/generators/modules/GenerateModuleH').generate],
};

function generateLibrary(libraryName, sourceDirectory) {
  if (!fs.readdirSync(sourceDirectory, {recursive: true}).some(file => /\.(js|ts)$/.test(file))) {
    return;
  }
  const schema = combineSchemasInFileList([sourceDirectory], null, /\/__tests__\//, libraryName);
  if (Object.keys(schema.modules).length === 0) {
    return;
  }
  const outputs = {
    componentsAndroid: path.join(outputRoot, 'react/renderer/components', libraryName),
    modulesCxx: path.join(outputRoot, libraryName),
  };
  for (const [kind, outputDirectory] of Object.entries(outputs)) {
    fs.rmSync(outputDirectory, {recursive: true, force: true});
    const succeeded = RNCodegen.generate(
      {libraryName, schema, outputDirectory, libraryGenerators: {[kind]: generators[kind]}},
      {generators: [kind]},
    );
    if (!succeeded) {
      throw new Error(`Codegen failed for ${libraryName} ${kind}`);
    }
  }
}

generateLibrary('FBReactNativeSpec', path.join(reactNativeRoot, 'src'));
generateLibrary('ReactNativeGodotSpec', path.join(packageRoot, 'specs'));
