'use strict';

import ReactNativeStyleAttributes, {
  colorAttribute,
} from 'react-native/Libraries/Components/View/ReactNativeStyleAttributes';

const bubble = name => ({
  phasedRegistrationNames: {
    captured: name + 'Capture',
    bubbled: name,
  },
});

const enterLeave = name => ({
  phasedRegistrationNames: {
    captured: name + 'Capture',
    bubbled: name,
    skipBubbling: true,
  },
});

const bubblingEventTypes = {
  topBlur: bubble('onBlur'),
  topClick: bubble('onClick'),
  topFocus: bubble('onFocus'),
  topKeyDown: bubble('onKeyDown'),
  topKeyUp: bubble('onKeyUp'),
  topTouchCancel: bubble('onTouchCancel'),
  topTouchEnd: bubble('onTouchEnd'),
  topTouchMove: bubble('onTouchMove'),
  topTouchStart: bubble('onTouchStart'),
  topPointerCancel: bubble('onPointerCancel'),
  topPointerDown: bubble('onPointerDown'),
  topPointerEnter: enterLeave('onPointerEnter'),
  topPointerLeave: enterLeave('onPointerLeave'),
  topPointerMove: bubble('onPointerMove'),
  topPointerOut: bubble('onPointerOut'),
  topPointerOver: bubble('onPointerOver'),
  topPointerUp: bubble('onPointerUp'),
  topGotPointerCapture: bubble('onGotPointerCapture'),
  topLostPointerCapture: bubble('onLostPointerCapture'),
};

const directEventTypes = {
  topLayout: {registrationName: 'onLayout'},
};

const validAttributes = {
  ...ReactNativeStyleAttributes,
  style: ReactNativeStyleAttributes,
  backgroundColor: colorAttribute,
  borderColor: colorAttribute,
  color: colorAttribute,
  display: true,
  focusable: true,
  hitSlop: true,
  nativeID: true,
  onClick: true,
  onFocus: true,
  onBlur: true,
  onKeyDown: true,
  onKeyUp: true,
  onLayout: true,
  onPointerCancel: true,
  onPointerDown: true,
  onPointerEnter: true,
  onPointerLeave: true,
  onPointerMove: true,
  onPointerOut: true,
  onPointerOver: true,
  onPointerUp: true,
  onTouchCancel: true,
  onTouchEnd: true,
  onTouchMove: true,
  onTouchStart: true,
  opacity: true,
  overflow: true,
  pointerEvents: true,
  testID: true,
};

export default {bubblingEventTypes, directEventTypes, validAttributes};
