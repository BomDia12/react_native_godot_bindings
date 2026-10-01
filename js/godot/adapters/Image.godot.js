'use strict';

import React from 'react';
import resolveAssetSource from './resolveAssetSource.godot';

const unsupported = operation => global.__godotUnsupported(operation);

const Image = React.forwardRef(function Image() {
  unsupported('Image.render');
  return null;
});

Image.resolveAssetSource = resolveAssetSource;
Image.getSize = () => unsupported('Image.getSize');
Image.getSizeWithHeaders = () => unsupported('Image.getSizeWithHeaders');
Image.prefetch = () => unsupported('Image.prefetch');
Image.prefetchWithMetadata = () => unsupported('Image.prefetchWithMetadata');
Image.abortPrefetch = () => unsupported('Image.abortPrefetch');
Image.queryCache = () => unsupported('Image.queryCache');

export default Image;
