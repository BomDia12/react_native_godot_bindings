import AsyncStorage from '@react-native-async-storage/async-storage';
import React, {useRef} from 'react';
import {Text, View} from 'react-native';
import GodotProgressBar from 'react-native-godot/GodotProgressBar';
import GodotRichText, {GodotRichTextCommands} from 'react-native-godot/GodotRichText';
import GodotTextureProgressBar from 'react-native-godot/GodotTextureProgressBar';
import GodotVideo, {GodotVideoCommands} from 'react-native-godot/GodotVideo';
import GodotViewportView from 'react-native-godot/GodotViewportView';
import {GodotAudio, useAudioBus} from 'react-native-godot/audio';
import {setLocale, tr, trN, useTranslation} from 'react-native-godot/i18n';
import {GodotInput, useInputAction, useLastInputDevice} from 'react-native-godot/input';
import {useGodotDrag} from 'react-native-godot/root';
import {color} from 'react-native-godot/values';

export function GameUI() {
  const rich = useRef<any>(null);
  const video = useRef<any>(null);
  const {tr: translate, locale} = useTranslation();
  const device: 'keyboard' | 'mouse' | 'joypad' | 'touch' = useLastInputDevice();
  const master = useAudioBus('Master');
  const dragged = useGodotDrag();
  useInputAction('jump', event => event.pressed);
  GodotInput.captureNextEvent({devices: ['keyboard']}).then(event => GodotInput.setActionEvents('jump', [event]));
  GodotInput.vibrateJoypad(0, {weak: 0.5, durationMs: 100});
  GodotAudio.play('res://click.ogg', {bus: 'UI', volumeDb: -6});
  master.setVolume(master.volume / 2);
  setLocale(locale);
  AsyncStorage.setItem('key', tr('A') + trN('B', 'C', 2));
  GodotRichTextCommands.reveal(rich.current, 1, 2);
  GodotVideoCommands.seek(video.current, 1);
  return (
    <View
      godotThemeType="Panel"
      godotStyleBox="res://frame.tres"
      godotMaterial="res://fx.tres"
      godotShaderParameters={{tint: color(1, 0, 0), amount: 0.5}}
      godotDragData={{type: 'item', id: 1}}
      godotDropTypes={['item']}
      onGodotDrop={event => event.nativeEvent.data.type}
      onGodotDragEnd={event => event.nativeEvent.successful}
      tooltip={device}>
      <Text godotThemeType="Title">{translate('T')}{String(dragged?.type)}</Text>
      <GodotProgressBar value={1} fillMode="endToBegin" />
      <GodotTextureProgressBar fillMode="clockwise" radialCenterOffset={{x: 1, y: 2}} stretchMargins={{left: 1}} />
      <GodotRichText ref={rich} bbcode="[b]x[/b]" customEffects={['res://e.tres']} onMetaClick={event => event.nativeEvent.meta} />
      <GodotViewportView viewport="map" resizeMode="contain" matchSize forwardInput />
      <GodotVideo ref={video} source="res://a.ogv" autoplay onEnd={() => {}} />
    </View>
  );
}
