'use strict';
import resolveAssetSource from './adapters/resolveAssetSource.godot';
export function fontFamily(asset) {
  const source = resolveAssetSource(asset);
  if (source == null || typeof source.uri !== 'string' || !/\.(ttf|otf|woff|woff2)$/i.test(source.uri)) {
    throw new TypeError('Godot fontFamily requires a staged TTF, OTF, WOFF or WOFF2 asset.');
  }
  return source.uri;
}
