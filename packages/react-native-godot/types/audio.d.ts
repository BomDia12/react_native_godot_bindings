import type {ImageSourcePropType} from 'react-native';

// A res:// path to an AudioStream, or a required asset.
export type GodotAudioSource = string | ImageSourcePropType;
export type GodotAudioBus = {name: string; volume: number; muted: boolean};

export const GodotAudio: {
  play(source: GodotAudioSource, options?: {bus?: string; volumeDb?: number; pitchScale?: number}): void;
  preload(sources: readonly GodotAudioSource[]): Promise<void>;
  stopAll(bus?: string): void;
  getBuses(): GodotAudioBus[];
  setBusVolume(bus: string, linear: number): void;
  setBusMute(bus: string, muted: boolean): void;
  onBusesChanged(handler: (buses: GodotAudioBus[]) => void): {remove(): void};
};
export function useAudioBus(name: string): {
  volume: number;
  muted: boolean;
  setVolume(linear: number): void;
  setMuted(muted: boolean): void;
};
