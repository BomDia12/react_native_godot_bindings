'use strict';

const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

function canonicalScale(scale) {
  if (!Number.isFinite(scale) || scale <= 0) {
    throw new Error('Invalid Metro asset scale: ' + scale);
  }
  return String(Number(scale));
}

function validatePart(value, label, pattern) {
  if (typeof value !== 'string' || !pattern.test(value)) {
    throw new Error('Invalid Metro asset ' + label + ': ' + String(value));
  }
}

function godotAssetMetadata(asset) {
  validatePart(asset.hash, 'hash', /^[a-f0-9]+$/i);
  validatePart(asset.type, 'extension', /^[a-z0-9]+$/i);
  if (!Array.isArray(asset.files) || asset.files.length !== asset.scales.length) {
    throw new Error('Metro asset files/scales do not match');
  }
  const variants = asset.scales.map((scale, index) => ({
    scale,
    uri:
      'res://dist/assets/' +
      asset.hash +
      '/' +
      canonicalScale(scale) +
      '.' +
      asset.type,
    source: asset.files[index],
  }));
  return {
    variants: variants.map(({scale, uri}) => ({scale, uri})),
  };
}

function assetPlugin(asset) {
  return {...asset, godotAsset: godotAssetMetadata(asset)};
}

function hashFile(filePath) {
  return crypto.createHash('sha256').update(fs.readFileSync(filePath)).digest('hex');
}

function stageAssets(assets, stagingRoot) {
  const manifest = [];
  const destinations = new Map();
  for (const asset of assets) {
    const metadata = godotAssetMetadata(asset);
    const variants = metadata.variants.map((variant, index) => {
      const source = asset.files[index];
      const stat = fs.statSync(source);
      if (!stat.isFile()) {
        throw new Error('Metro asset source is not a regular file: ' + source);
      }
      const relative = variant.uri.slice('res://dist/'.length);
      const destination = path.join(stagingRoot, relative);
      const digest = hashFile(source);
      const prior = destinations.get(destination);
      if (prior != null && prior !== digest) {
        throw new Error('Asset destination collision: ' + variant.uri);
      }
      destinations.set(destination, digest);
      fs.mkdirSync(path.dirname(destination), {recursive: true});
      if (!fs.existsSync(destination)) {
        fs.copyFileSync(source, destination);
      }
      return {...variant, sha256: digest};
    });
    manifest.push({
      hash: asset.hash,
      name: asset.name,
      type: asset.type,
      width: asset.width,
      height: asset.height,
      scales: [...asset.scales],
      variants,
    });
  }
  manifest.sort((a, b) =>
    (a.name + '.' + a.type + ':' + a.hash).localeCompare(
      b.name + '.' + b.type + ':' + b.hash,
    ),
  );
  return manifest;
}

module.exports = assetPlugin;
module.exports.canonicalScale = canonicalScale;
module.exports.godotAssetMetadata = godotAssetMetadata;
module.exports.stageAssets = stageAssets;
