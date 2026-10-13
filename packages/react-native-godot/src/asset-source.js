'use strict';

import Platform from 'react-native/Libraries/Utilities/Platform';

const {pickScale} = require('../asset-scale.cjs');

export function godotAssetSource({asset}) {
  if (!asset?.godotAsset?.variants) {
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
    uri: selected.uri,
    width: asset.width,
    height: asset.height,
    scale,
  };
}
