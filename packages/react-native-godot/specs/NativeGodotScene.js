/**
 * @flow strict
 * @format
 */

import type {RootTag, TurboModule} from 'react-native/Libraries/TurboModule/RCTExport';
import type {EventEmitter, UnsafeMixed, UnsafeObject} from 'react-native/Libraries/Types/CodegenTypes';

import * as TurboModuleRegistry from 'react-native/Libraries/TurboModule/TurboModuleRegistry';

export type SceneBinding = {
  ready: boolean,
  binding: ?string,
  sequence: number,
  capability?: string,
  schemaVersion?: number,
};

export type SceneChange = {
  rootTag: number,
  binding: ?string,
  ready: boolean,
  sequence: number,
  event: string,
  payload?: UnsafeObject,
};

export interface Spec extends TurboModule {
  +getBinding: (rootTag: RootTag) => SceneBinding;
  +read: (rootTag: RootTag, binding: string) => UnsafeObject;
  +call: (rootTag: RootTag, binding: string, command: string, args: Array<UnsafeMixed>) => UnsafeMixed;
  +callAsync: (rootTag: RootTag, binding: string, command: string, args: Array<UnsafeMixed>) => Promise<UnsafeMixed>;
  +onChanged: EventEmitter<SceneChange>;
}

export default (TurboModuleRegistry.getEnforcing<Spec>('GodotSceneCxx'): Spec);
