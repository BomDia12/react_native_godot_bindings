'use strict';

const {AssetRegistry} = require('react-native/src/private/assets/AssetRegistry');
const {pickScale} = require('../asset-scale.cjs');

let transformers = [];

function validateLocalURI(uri) {
  if (typeof uri !== 'string' || uri.includes('\u0000') || uri.includes('\\')) {
    throw new TypeError('Godot local asset URI is invalid.');
  }
  if (
    uri.startsWith('/') ||
    uri.startsWith('file://') ||
    /^[A-Za-z]:\//.test(uri)
  ) {
    throw new TypeError('Godot local asset URI must use res:// or user://.');
  }
  if (!uri.startsWith('res://') && !uri.startsWith('user://')) {
    return;
  }
  const path = uri.slice(uri.indexOf('://') + 3);
  if (path.length === 0 || path.split('/').includes('..')) {
    throw new TypeError('Godot local asset URI contains an invalid path.');
  }
}

function resolveAssetSource(source) {
  if (source == null) {
    return source;
  }
  if (typeof source === 'object') {
    validateLocalURI(source.uri);
    return source;
  }
  const asset = AssetRegistry.getAssetByID(source);
  if (!asset?.godotAsset?.variants) {
    return null;
  }
  const resolver = {asset};
  for (const transformer of transformers) {
    const transformed = transformer(resolver);
    if (transformed != null) {
      validateLocalURI(transformed.uri);
      return transformed;
    }
  }
  const scale = pickScale(
    asset.godotAsset.variants.map(variant => variant.scale),
    require('./Platform.godot').default.constants.assetScale,
  );
  const selected = asset.godotAsset.variants.find(
    variant => variant.scale === scale,
  );
  if (selected == null) {
    throw new Error('Godot asset manifest has no selected scale variant.');
  }
  return {
    __packager_asset: true,
    uri: selected.uri,
    width: asset.width,
    height: asset.height,
    scale,
  };
}

resolveAssetSource.pickScale = pickScale;
resolveAssetSource.setCustomSourceTransformer = transformer => {
  transformers = [transformer];
};
resolveAssetSource.addCustomSourceTransformer = transformer => {
  transformers.push(transformer);
};

export default resolveAssetSource;
