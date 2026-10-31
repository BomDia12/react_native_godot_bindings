/**
 * @flow strict
 * @format
 */

import type {RootTag, TurboModule} from 'react-native/Libraries/TurboModule/RCTExport';
import type {EventEmitter} from 'react-native/Libraries/Types/CodegenTypes';

import * as TurboModuleRegistry from 'react-native/Libraries/TurboModule/TurboModuleRegistry';

export type RootState = {
  visible: boolean,
  processing: boolean,
  focused: boolean,
};

export type RootStateChange = {
  rootTag: number,
  visible: boolean,
  processing: boolean,
  focused: boolean,
};

export interface Spec extends TurboModule {
  +getRootState: (rootTag: RootTag) => RootState;
  +setBackHandlerEnabled: (enabled: boolean) => void;
  +invokeDefaultBackPressHandler: () => void;
  +onRootStateChanged: EventEmitter<RootStateChange>;
}

export default (TurboModuleRegistry.getEnforcing<Spec>('GodotHostCxx'): Spec);
