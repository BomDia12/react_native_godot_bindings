'use strict';

import NativeAccessibilityManager from 'react-native/Libraries/Components/AccessibilityInfo/NativeAccessibilityManager';

// Only focus events have a Godot meaning, as on iOS.
export default function legacySendAccessibilityEvent(reactTag, eventType) {
  if (eventType === 'focus' && NativeAccessibilityManager) {
    NativeAccessibilityManager.setAccessibilityFocus(reactTag);
  }
}
