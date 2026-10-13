'use strict';
import React from 'react';
import * as NativeComponentRegistry from 'react-native/Libraries/NativeComponent/NativeComponentRegistry';
const NativeWindow = NativeComponentRegistry.get('GodotWindow', () => ({
  uiViewClassName: 'GodotWindow',
  directEventTypes: {
    topShow: {registrationName: 'onShow'}, topDismiss: {registrationName: 'onDismiss'},
    topRequestClose: {registrationName: 'onRequestClose'}, topResize: {registrationName: 'onResize'},
  },
  validAttributes: {windowWidth: true, windowHeight: true, title: true, visible: true,
    onShow: true, onDismiss: true, onRequestClose: true, onResize: true},
}));
export default React.forwardRef(function GodotWindow({width = 640, height = 480, ...props}, ref) {
  return <NativeWindow {...props} ref={ref} windowWidth={width} windowHeight={height} />;
});
