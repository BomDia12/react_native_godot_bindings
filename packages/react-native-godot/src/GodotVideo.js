'use strict';

import React from 'react';
import resolveAssetSource from 'react-native/Libraries/Image/resolveAssetSource';
import NativeGodotVideo, {Commands} from '../specs/GodotVideoNativeComponent';

const GodotVideo = React.forwardRef(function GodotVideo({source, ...props}, ref) {
  const uri = typeof source === 'string' ? source : resolveAssetSource(source)?.uri;
  return <NativeGodotVideo {...props} ref={ref} source={uri ?? ''} />;
});

export default GodotVideo;
export const GodotVideoCommands = Commands;
