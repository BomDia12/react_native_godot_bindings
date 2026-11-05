'use strict';

import Platform from 'react-native/Libraries/Utilities/Platform';

const {metroAssetResourcePath} = require('../asset-path.cjs');
const {pickScale} = require('../asset-scale.cjs');

// A bundle Metro served gets its assets from Metro, as on other platforms, so
// assets added while developing need no rebuild. A bundle file finds them
// next to itself, where the build staged them.
export function godotAssetSource(resolver) {
  const {asset, jsbundleUrl} = resolver;
  if (!asset?.godotAsset?.variants || resolver.isLoadedFromServer() || jsbundleUrl == null) {
    return null;
  }
  const scale = pickScale(
    asset.godotAsset.variants.map(variant => variant.scale),
    Platform.constants.assetScale,
  );
  const selected = asset.godotAsset.variants.find(
    variant => variant.scale === scale,
  );
  if (selected == null) {
    throw new Error('Godot asset manifest has no selected scale variant.');
  }
  return {
    __packager_asset: true,
    uri: jsbundleUrl + selected.path,
    width: asset.width,
    height: asset.height,
    scale,
  };
}

// The path Godot loads a non-image asset from. Images can come over HTTP from Metro,
// but audio and video streams can only load as resources.
export function godotResourcePath(source) {
  if (typeof source === 'string') {
    return source;
  }
  const uri = require('react-native/Libraries/Image/resolveAssetSource').default(source)?.uri ?? '';
  if (uri.startsWith('res://') || uri.startsWith('user://') || typeof source !== 'number') {
    return uri;
  }
  const asset = require('react-native/src/private/assets/AssetRegistry').AssetRegistry.getAssetByID(source);
  return metroAssetResourcePath(asset, Platform.constants.projectRoot) ?? uri;
}
