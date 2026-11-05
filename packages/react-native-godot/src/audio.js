'use strict';

import {useEffect, useState} from 'react';
import NativeGodotAudio from '../specs/NativeGodotAudio';
import {godotResourcePath} from './asset-source';

export const GodotAudio = {
  play: (source, {bus = 'Master', volumeDb = 0, pitchScale = 1} = {}) => NativeGodotAudio.play(godotResourcePath(source), bus, volumeDb, pitchScale),
  preload: sources => NativeGodotAudio.preload(sources.map(godotResourcePath)),
  stopAll: bus => NativeGodotAudio.stopAll(bus ?? null),
  getBuses: () => NativeGodotAudio.getBuses(),
  setBusVolume: (bus, linear) => NativeGodotAudio.setBusVolume(bus, linear),
  setBusMute: (bus, muted) => NativeGodotAudio.setBusMute(bus, muted),
  onBusesChanged: handler => NativeGodotAudio.onBusesChanged(handler),
};

export function useAudioBus(name) {
  const find = buses => buses.find(bus => bus.name === name);
  const [bus, setBus] = useState(() => find(NativeGodotAudio.getBuses()));
  useEffect(() => {
    setBus(find(NativeGodotAudio.getBuses()));
    const subscription = NativeGodotAudio.onBusesChanged(buses => setBus(find(buses)));
    return () => subscription.remove();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [name]);
  return {
    volume: bus?.volume ?? 0,
    muted: bus?.muted ?? false,
    setVolume: linear => NativeGodotAudio.setBusVolume(name, linear),
    setMuted: muted => NativeGodotAudio.setBusMute(name, muted),
  };
}
