/**
 * @flow strict
 * @format
 */

import type {TurboModule} from 'react-native/Libraries/TurboModule/RCTExport';
import type {EventEmitter, UnsafeMixed} from 'react-native/Libraries/Types/CodegenTypes';

import * as TurboModuleRegistry from 'react-native/Libraries/TurboModule/TurboModuleRegistry';

export interface Spec extends TurboModule {
  +watchAction: (name: string) => void;
  +unwatchAction: (name: string) => void;
  +getActions: () => UnsafeMixed;
  +getActionEvents: (name: string) => UnsafeMixed;
  +setActionEvents: (name: string, events: UnsafeMixed) => void;
  +resetAction: (name: string) => void;
  +captureNextEvent: (devices: UnsafeMixed) => Promise<UnsafeMixed>;
  +cancelCapture: () => void;
  +getLastDevice: () => string;
  +getJoypads: () => UnsafeMixed;
  +vibrateJoypad: (device: number, weak: number, strong: number, durationMs: number) => void;
  +stopJoypadVibration: (device: number) => void;
  +onAction: EventEmitter<UnsafeMixed>;
  +onLastDeviceChanged: EventEmitter<string>;
  +onJoypadsChanged: EventEmitter<UnsafeMixed>;
}

export default (TurboModuleRegistry.getEnforcing<Spec>('GodotInputCxx'): Spec);
