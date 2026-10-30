/**
 * @flow strict
 * @format
 */

import type {RootTag, TurboModule} from 'react-native/Libraries/TurboModule/RCTExport';
import type {EventEmitter, UnsafeMixed} from 'react-native/Libraries/Types/CodegenTypes';

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

// A message for one root carries its tag; an application message carries null.
export type GodotMessage = {
  rootTag: ?number,
  name: string,
  payload?: UnsafeMixed,
};

export interface Spec extends TurboModule {
  +getRootState: (rootTag: RootTag) => RootState;
  +setBackHandlerEnabled: (enabled: boolean) => void;
  +postMessage: (rootTag: ?number, name: string, payload: UnsafeMixed) => void;
  +invokeDefaultBackPressHandler: () => void;
  +onRootStateChanged: EventEmitter<RootStateChange>;
  +onMessage: EventEmitter<GodotMessage>;
}

export default (TurboModuleRegistry.getEnforcing<Spec>('GodotHostCxx'): Spec);
