'use strict';

import React from 'react';
import resolveAssetSource from 'react-native/Libraries/Image/resolveAssetSource';
import NativeGodotTextureProgressBar from '../specs/GodotTextureProgressBarNativeComponent';

const GodotTextureProgressBar = React.forwardRef(function GodotTextureProgressBar(
  {underTexture, progressTexture, overTexture, radialCenterOffset, stretchMargins, ...props},
  ref,
) {
  return (
    <NativeGodotTextureProgressBar
      {...props}
      ref={ref}
      underTexture={resolveAssetSource(underTexture)}
      progressTexture={resolveAssetSource(progressTexture)}
      overTexture={resolveAssetSource(overTexture)}
      radialCenterOffsetX={radialCenterOffset?.x}
      radialCenterOffsetY={radialCenterOffset?.y}
      stretchMarginLeft={stretchMargins?.left}
      stretchMarginTop={stretchMargins?.top}
      stretchMarginRight={stretchMargins?.right}
      stretchMarginBottom={stretchMargins?.bottom}
    />
  );
});

export default GodotTextureProgressBar;
