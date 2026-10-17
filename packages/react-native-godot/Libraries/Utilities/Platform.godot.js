'use strict';

import * as TurboModuleRegistry from 'react-native/Libraries/TurboModule/TurboModuleRegistry';

const Platform = {
  __constants: null,
  OS: 'godot',
  get constants() {
    if (this.__constants == null) {
      this.__constants =
        TurboModuleRegistry.getEnforcing('PlatformConstants').getConstants();
    }
    return this.__constants;
  },
  get Version() {
    return this.constants.Version;
  },
  get isTesting() {
    return __DEV__ ? this.constants.isTesting : false;
  },
  get isDisableAnimations() {
    return this.constants.isDisableAnimations ?? this.isTesting;
  },
  get isTV() {
    return false;
  },
  get isVision() {
    return false;
  },
  select(spec) {
    if ('godot' in spec) {
      return spec.godot;
    }
    if ('native' in spec) {
      return spec.native;
    }
    return spec.default;
  },
};

export default Platform;
