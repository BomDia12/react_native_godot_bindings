import React from 'react';
import {Platform, View} from 'react-native';
import {GodotAppRegistry, useGodotAlert} from 'react-native-godot/app-registry';
import GodotWindow from 'react-native-godot/GodotWindow';
import {fontFamily} from 'react-native-godot/fonts';
import {call, onChanged, useGodotScene} from 'react-native-godot/scene';
import 'react-native-godot/setup';
import {color, int64} from 'react-native-godot/values';

type Enemy = {health: number};

function Probe({rootTag}: {rootTag: number}) {
  const scene = useGodotScene<Enemy>(rootTag);
  const alert = useGodotAlert();
  const health: number | undefined = scene?.value.health;
  const os: string = Platform.OS;
  onChanged(rootTag, change => change.sequence).remove();
  return (
    <View onRightClick={event => event.nativeEvent.pageX} godotContextMenu={[{id: 'heal', label: 'Heal'}]}>
      <GodotWindow title="Details" width={320} onResize={event => event.nativeEvent.width}>
        <View onTouchEnd={() => scene && call<Enemy>(rootTag, scene.handle, 'heal', [health ?? 0, color(1, 0, 0), int64(7), os, fontFamily(1)])} />
      </GodotWindow>
      <View onTouchEnd={() => alert('Title')} />
    </View>
  );
}

GodotAppRegistry.registerComponent('Probe', () => Probe);

// @ts-expect-error scene snapshots are typed by the caller
const wrong: string = useGodotScene<Enemy>(1)?.value.health;
