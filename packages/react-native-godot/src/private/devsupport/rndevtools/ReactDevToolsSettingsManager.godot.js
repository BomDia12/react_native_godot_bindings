'use strict';

import NativeReactDevToolsSettingsManager from 'react-native/src/private/devsupport/rndevtools/specs/NativeReactDevToolsSettingsManager';

export function setGlobalHookSettings(settings) {
  NativeReactDevToolsSettingsManager?.setGlobalHookSettings(settings);
}

export function getGlobalHookSettings() {
  return NativeReactDevToolsSettingsManager?.getGlobalHookSettings();
}
