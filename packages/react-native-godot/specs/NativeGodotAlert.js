/**
 * @flow strict
 * @format
 */

import type {RootTag, TurboModule} from 'react-native/Libraries/TurboModule/RCTExport';
import type {EventEmitter} from 'react-native/Libraries/Types/CodegenTypes';

import * as TurboModuleRegistry from 'react-native/Libraries/TurboModule/TurboModuleRegistry';

export type AlertButton = {
  text: string,
  style: string,
};

export type AlertRequest = {
  title: string,
  message: string,
  cancelable: boolean,
  buttons: Array<AlertButton>,
};

export type AlertResult = {
  buttonId: ?number,
  dismissed: boolean,
};

export type AlertPresentation = {
  requestId: string,
  rootTag: number,
  payload: AlertRequest,
};

export interface Spec extends TurboModule {
  +request: (rootTag: RootTag, payload: AlertRequest, mode: string) => Promise<AlertResult>;
  +reply: (rootTag: RootTag, requestId: string, result: AlertResult) => void;
  +decline: (rootTag: RootTag, requestId: string) => void;
  +onPresent: EventEmitter<AlertPresentation>;
}

export default (TurboModuleRegistry.getEnforcing<Spec>('GodotAlert'): Spec);
