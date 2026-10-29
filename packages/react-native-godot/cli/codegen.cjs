'use strict';

const fs = require('fs');
const path = require('path');

const component = name => require(`@react-native/codegen/lib/generators/components/${name}`).generate;

// Upstream keys its output folders by generator name; these keys reuse the
// componentsAndroid and modulesCxx folders with only the C++ generators.
function generators() {
  return {
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
}

// Writes the C++ that React Native's codegen derives from the specs in
// sourceDirectory: components under react/renderer/components/<library>/ and
// TurboModules under <library>/, as React Native's own builds lay them out.
function generateLibrary(libraryName, sourceDirectory, outputRoot) {
  const RNCodegen = require('@react-native/codegen/lib/generators/RNCodegen');
  const {combineSchemasInFileList} = require('@react-native/codegen/lib/cli/combine/combine-js-to-schema');
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
  const libraryGenerators = generators();
  for (const [kind, outputDirectory] of Object.entries(outputs)) {
    fs.rmSync(outputDirectory, {recursive: true, force: true});
    const succeeded = RNCodegen.generate(
      {libraryName, schema, outputDirectory, libraryGenerators: {[kind]: libraryGenerators[kind]}},
      {generators: [kind]},
    );
    if (!succeeded) {
      throw new Error(`Codegen failed for ${libraryName} ${kind}`);
    }
  }
}

module.exports = {generateLibrary};
