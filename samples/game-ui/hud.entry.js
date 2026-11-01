import AsyncStorage, {createAsyncStorage} from '@react-native-async-storage/async-storage';
import Slider from '@react-native-community/slider';
import React, {useEffect, useRef, useState} from 'react';
import {AppRegistry, Pressable, StyleSheet, Text, View} from 'react-native';
import GodotProgressBar from 'react-native-godot/GodotProgressBar';
import GodotRichText from 'react-native-godot/GodotRichText';
import GodotTextureProgressBar from 'react-native-godot/GodotTextureProgressBar';
import GodotViewportView from 'react-native-godot/GodotViewportView';
import {GodotAudio, useAudioBus} from 'react-native-godot/audio';
import {setLocale, useTranslation} from 'react-native-godot/i18n';
import {GodotInput} from 'react-native-godot/input';
import {usePostGodotMessage, useGodotMessage} from 'react-native-godot/messages';
import {useGodotDrag} from 'react-native-godot/root';

const FRAME = 'res://game/ui/frame.tres';
const POTION_ACTION = 'game_potion';
const settings = createAsyncStorage('game-settings');
const observations = (global.__gameHud = {AsyncStorage, GodotInput, settings, drops: []});

function PartyStatus() {
  const [party, setParty] = useState({health: 0, maximum: 1});
  const post = usePostGodotMessage();
  useGodotMessage('party', setParty);
  useEffect(() => post('ready'), [post]);
  observations.party = party;
  return (
    <View nativeID="party" tooltip="The health of every enemy" style={styles.row}>
      <GodotProgressBar nativeID="party-health" value={party.health} maximumValue={party.maximum} style={styles.grow} />
      <GodotTextureProgressBar
        nativeID="party-ring"
        value={party.health}
        maximumValue={party.maximum}
        fillMode="clockwise"
        underTexture={{uri: 'res://game/ui/ring_bg.png'}}
        progressTexture={{uri: 'res://game/ui/ring.png'}}
        style={styles.ring}
      />
    </View>
  );
}

function Slot({index, item, onDrop}) {
  const dragged = useGodotDrag();
  return (
    <View
      nativeID={`slot-${index}`}
      godotStyleBox={FRAME}
      godotDragData={item ? {type: 'item', from: index, name: item} : undefined}
      godotDropTypes={['item']}
      tooltip={item ?? 'Empty'}
      onGodotDrop={event => onDrop(event.nativeEvent.data.from, index)}
      style={[styles.slot, dragged && !item && styles.slotOpen]}>
      <Text style={styles.slotText}>{item ?? ''}</Text>
    </View>
  );
}

function Slots() {
  const [slots, setSlots] = useState(['Sword', null, 'Shield']);
  const move = (from, to) => {
    observations.drops.push([from, to]);
    setSlots(current => {
      const next = [...current];
      next[to] = current[from];
      next[from] = current[to];
      return next;
    });
  };
  observations.slots = slots;
  return (
    <View style={styles.row}>
      {slots.map((item, index) => (
        <Slot key={index} index={index} item={item} onDrop={move} />
      ))}
    </View>
  );
}

function Settings() {
  const {tr, locale} = useTranslation();
  const master = useAudioBus('Master');
  const [binding, setBinding] = useState(() => GodotInput.getActionEvents(POTION_ACTION)[0]?.label ?? '');
  const restored = useRef(false);
  observations.locale = locale;
  observations.volumeLabel = tr('VOLUME');
  observations.languageLabel = tr('LANGUAGE');
  observations.volume = master.volume;
  observations.binding = binding;
  useEffect(() => {
    settings.getMany(['volume', 'locale', 'binding']).then(saved => {
      if (saved.volume != null) {
        master.setVolume(Number(saved.volume));
      }
      if (saved.locale != null) {
        setLocale(saved.locale);
      }
      if (saved.binding != null) {
        const event = JSON.parse(saved.binding);
        GodotInput.setActionEvents(POTION_ACTION, [event]);
        setBinding(event.label);
      }
      restored.current = true;
      observations.restored = true;
    });
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);
  const changeLocale = next => {
    setLocale(next);
    settings.setItem('locale', next);
  };
  const rebind = async () => {
    const event = await GodotInput.captureNextEvent({devices: ['keyboard', 'joypad']});
    GodotInput.setActionEvents(POTION_ACTION, [event]);
    setBinding(event.label);
    settings.setItem('binding', JSON.stringify(event));
  };
  observations.rebind = rebind;
  observations.changeLocale = changeLocale;
  observations.setVolume = next => {
    master.setVolume(next);
    settings.setItem('volume', String(next));
  };
  return (
    <View godotStyleBox={FRAME} style={styles.panel}>
      <Text style={styles.label}>{tr('VOLUME')}</Text>
      <Slider
        nativeID="volume"
        value={master.volume}
        minimumValue={0}
        maximumValue={1}
        step={0.05}
        onSlidingComplete={observations.setVolume}
        style={styles.slider}
      />
      <Text style={styles.label}>{tr('LANGUAGE')}</Text>
      <View style={styles.row}>
        {['en', 'pt_BR'].map(option => (
          <Pressable key={option} onPress={() => changeLocale(option)} style={styles.button}>
            <Text style={styles.label}>{option}</Text>
          </Pressable>
        ))}
      </View>
      <Pressable nativeID="rebind" onPress={rebind} style={styles.button}>
        <Text style={styles.label}>
          {tr('REBIND')}: {binding}
        </Text>
      </Pressable>
    </View>
  );
}

function GameHud() {
  const {tr} = useTranslation();
  return (
    <View style={styles.hud}>
      <PartyStatus />
      <GodotViewportView nativeID="minimap" viewport="minimap" resizeMode="contain" tooltip="Minimap" style={styles.minimap} />
      <GodotRichText nativeID="objective" bbcode={`[b]${tr('SLOTS')}[/b]`} />
      <Slots />
      <Settings />
    </View>
  );
}

const styles = StyleSheet.create({
  hud: {flex: 1, padding: 8, gap: 8, backgroundColor: '#18202b'},
  row: {flexDirection: 'row', alignItems: 'center', gap: 8},
  grow: {flex: 1},
  ring: {width: 32, height: 32},
  minimap: {height: 120, alignSelf: 'stretch'},
  slot: {width: 88, height: 56, alignItems: 'center', justifyContent: 'center'},
  slotOpen: {opacity: 0.6},
  slotText: {color: '#ffffff'},
  panel: {padding: 12, gap: 6},
  label: {color: '#ffffff'},
  slider: {height: 24, alignSelf: 'stretch'},
  button: {padding: 6, backgroundColor: '#30638e'},
});

AppRegistry.registerComponent('GameHud', () => GameHud);
