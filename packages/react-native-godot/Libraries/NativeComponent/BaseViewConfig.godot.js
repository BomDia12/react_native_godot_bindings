'use strict';

import ReactNativeStyleAttributes, {
  colorAttribute,
} from 'react-native/Libraries/Components/View/ReactNativeStyleAttributes';

const optionalRecord = value => {
  if (value == null) {
    return null;
  }
  const result = {};
  for (const key of Object.keys(value)) {
    if (value[key] !== undefined) {
      result[key] = value[key];
    }
  }
  return result;
};

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
  topMiddleClick: bubble('onMiddleClick'),
  topRightClick: bubble('onRightClick'),
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
  topGodotContextMenuAction: {registrationName: 'onGodotContextMenuAction'},
  topAccessibilityAction: {registrationName: 'onAccessibilityAction'},
};

const validAttributes = {
  allowFontScaling: true,
  maxFontSizeMultiplier: true,
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
  onMiddleClick: true,
  onMiddleClickCapture: true,
  onRightClick: true,
  onRightClickCapture: true,
  godotContextMenu: true,
  onGodotContextMenuAction: true,
  accessible: true,
  accessibilityLabel: true,
  accessibilityRole: true,
  accessibilityHint: true,
  accessibilityState: {process: optionalRecord},
  accessibilityValue: {process: optionalRecord},
  accessibilityActions: {process: value => value?.map(optionalRecord)},
  accessibilityElementsHidden: true,
  importantForAccessibility: true,
  onAccessibilityAction: true,
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
