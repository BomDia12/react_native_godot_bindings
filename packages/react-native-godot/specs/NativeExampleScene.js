/**
 * @flow strict
 * @format
 */

import type {RootTag, TurboModule} from 'react-native/Libraries/TurboModule/RCTExport';
import type {EventEmitter} from 'react-native/Libraries/Types/CodegenTypes';

import * as TurboModuleRegistry from 'react-native/Libraries/TurboModule/TurboModuleRegistry';

export type CounterValue = {
  value: number,
  x: number,
  y: number,
};

export interface Spec extends TurboModule {
  +getConstants: () => {name: string, version: number};
  +read: (rootTag: RootTag) => CounterValue;
  +incrementLater: (rootTag: RootTag, amount: number) => Promise<CounterValue>;
  +onChanged: EventEmitter<CounterValue>;
}

export default (TurboModuleRegistry.getEnforcing<Spec>('ExampleScene'): Spec);
