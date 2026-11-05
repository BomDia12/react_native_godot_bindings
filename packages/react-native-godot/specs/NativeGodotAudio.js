/**
 * @flow strict
 * @format
 */

import type {TurboModule} from 'react-native/Libraries/TurboModule/RCTExport';
import type {EventEmitter, UnsafeMixed} from 'react-native/Libraries/Types/CodegenTypes';

import * as TurboModuleRegistry from 'react-native/Libraries/TurboModule/TurboModuleRegistry';

export interface Spec extends TurboModule {
  +play: (source: string, bus: string, volumeDb: number, pitchScale: number) => void;
  +preload: (sources: UnsafeMixed) => Promise<void>;
  +stopAll: (bus: ?string) => void;
  +getBuses: () => UnsafeMixed;
  +setBusVolume: (name: string, linear: number) => void;
  +setBusMute: (name: string, muted: boolean) => void;
  +onBusesChanged: EventEmitter<UnsafeMixed>;
}

export default (TurboModuleRegistry.getEnforcing<Spec>('GodotAudioCxx'): Spec);
