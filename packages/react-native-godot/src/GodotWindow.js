'use strict';
import React from 'react';
import GodotWindowNativeComponent from '../specs/GodotWindowNativeComponent';

export default React.forwardRef(function GodotWindow({width = 640, height = 480, ...props}, ref) {
  return <GodotWindowNativeComponent {...props} ref={ref} windowWidth={width} windowHeight={height} />;
});
