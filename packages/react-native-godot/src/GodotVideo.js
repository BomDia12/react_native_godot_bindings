'use strict';

import React from 'react';
import NativeGodotVideo, {Commands} from '../specs/GodotVideoNativeComponent';
import {godotResourcePath} from './asset-source';

const GodotVideo = React.forwardRef(function GodotVideo({source, ...props}, ref) {
  return <NativeGodotVideo {...props} ref={ref} source={godotResourcePath(source)} />;
});

export default GodotVideo;
export const GodotVideoCommands = Commands;
