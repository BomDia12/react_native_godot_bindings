'use strict';

// Metro serves an asset by its directory under the project, and Godot has already
// imported the file there, so during development the file is also a Godot resource.
function metroAssetResourcePath(asset, projectRoot) {
  const prefix = '/assets/';
  const location = asset?.httpServerLocation;
  if (typeof location !== 'string' || !location.startsWith(prefix) || location.includes('?') || typeof projectRoot !== 'string') {
    return null;
  }
  const root = projectRoot.endsWith('/') ? projectRoot : projectRoot + '/';
  return root + location.slice(prefix.length) + '/' + asset.name + '.' + asset.type;
}

module.exports = {metroAssetResourcePath};
