/**
 * @flow strict
 * @format
 */

import type {TurboModule} from 'react-native/Libraries/TurboModule/RCTExport';
import type {EventEmitter} from 'react-native/Libraries/Types/CodegenTypes';

import * as TurboModuleRegistry from 'react-native/Libraries/TurboModule/TurboModuleRegistry';

export interface Spec extends TurboModule {
  +translate: (message: string, context: string) => string;
  +translatePlural: (message: string, plural: string, n: number, context: string) => string;
  +getLocale: () => string;
  +setLocale: (locale: string) => void;
  +getLoadedLocales: () => $ReadOnlyArray<string>;
  +onLocaleChanged: EventEmitter<string>;
}

export default (TurboModuleRegistry.getEnforcing<Spec>('GodotTranslation'): Spec);
