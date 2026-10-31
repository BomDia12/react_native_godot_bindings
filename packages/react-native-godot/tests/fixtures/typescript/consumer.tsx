import React from 'react';
import {Platform, View} from 'react-native';
import {GodotAppRegistry, useGodotAlert} from 'react-native-godot/app-registry';
import GodotScene from 'react-native-godot/GodotScene';
import GodotWindow from 'react-native-godot/GodotWindow';
import {withGodotContract} from 'react-native-godot/contracts';
import {fontFamily} from 'react-native-godot/fonts';
import {postGodotMessage, useGodotMessage, usePostGodotMessage} from 'react-native-godot/messages';
import {getGodotModule, useGodotModule} from 'react-native-godot/modules';
import {useGodotRootState} from 'react-native-godot/root';
import {call, onChanged, useGodotScene} from 'react-native-godot/scene';
import 'react-native-godot/setup';
import {color, int64} from 'react-native-godot/values';

type Enemy = {health: number};

function Probe({rootTag}: {rootTag: number}) {
  const scene = useGodotScene<Enemy>(rootTag);
  const alert = useGodotAlert();
  const health: number | undefined = scene?.value.health;
  const os: string = Platform.OS;
  const paused: boolean = !useGodotRootState().processing;
  onChanged(rootTag, change => change.sequence).remove();
  return (
    <View onRightClick={event => event.nativeEvent.pageX} godotContextMenu={[{id: 'heal', label: 'Heal'}]}>
      <GodotWindow title="Details" width={320} onResize={event => event.nativeEvent.width}>
        <View onTouchEnd={() => scene && call<Enemy>(rootTag, scene.handle, 'heal', [health ?? 0, color(1, 0, 0), int64(7), os, fontFamily(1)])} />
      </GodotWindow>
      <View onTouchEnd={() => alert('Title')} />
      <GodotScene source="res://minimap.tscn" nativeID="minimap" onError={event => event.nativeEvent.message} />
      {paused ? <View nextFocusDown={1} hasTVPreferredFocus /> : null}
    </View>
  );
}

GodotAppRegistry.registerComponent('Probe', () => Probe);

function Scoreboard({score, onReset}: {score: number; onReset: (playerId: string) => void}) {
  const post = usePostGodotMessage();
  useGodotMessage<{points: number}>('scored', payload => post('seen', payload.points));
  const scores = useGodotModule<{best: number}>('Scores');
  const best: number | undefined = scores?.value.best;
  return <View onTouchEnd={() => onReset(String(score + (best ?? 0)))} />;
}

GodotAppRegistry.registerComponent('Scoreboard', () => withGodotContract(Scoreboard, {reset: 'onReset'}));
postGodotMessage('ready');
const total: number = getGodotModule('Scores').call<number>('total', 1, 2);

// @ts-expect-error scene snapshots are typed by the caller
const wrong: string = useGodotScene<Enemy>(1)?.value.health;
