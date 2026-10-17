/**
 * @flow strict
 * @format
 */

import type {TurboModule} from 'react-native/Libraries/TurboModule/RCTExport';

import * as TurboModuleRegistry from 'react-native/Libraries/TurboModule/TurboModuleRegistry';

export type ReactNativeVersion = {
  major: number,
  minor: number,
  patch: number,
  prerelease: ?string,
};

export type PlatformConstants = {
  isTesting: boolean,
  isDisableAnimations?: boolean,
  reactNativeVersion: ReactNativeVersion,
  Version: string,
  godotVersion: string,
  hostOS: string,
  assetScale: number,
};

export interface Spec extends TurboModule {
  +getConstants: () => PlatformConstants;
}

export default (TurboModuleRegistry.getEnforcing<Spec>(
  'PlatformConstants',
): Spec);
