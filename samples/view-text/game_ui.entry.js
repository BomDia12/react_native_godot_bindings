import AsyncStorage, {createAsyncStorage} from '@react-native-async-storage/async-storage';
import Slider from '@react-native-community/slider';
import React, {useRef, useState} from 'react';
import {AppRegistry, PanResponder, Pressable, StyleSheet, Text, TextInput, TurboModuleRegistry, View} from 'react-native';
import GodotProgressBar from 'react-native-godot/GodotProgressBar';
import GodotRichText, {GodotRichTextCommands} from 'react-native-godot/GodotRichText';
import GodotTextureProgressBar from 'react-native-godot/GodotTextureProgressBar';
import GodotVideo from 'react-native-godot/GodotVideo';
import GodotViewportView from 'react-native-godot/GodotViewportView';
import {GodotAudio, useAudioBus} from 'react-native-godot/audio';
import {getLoadedLocales, setLocale, tr, useTranslation} from 'react-native-godot/i18n';
import {GodotInput, useInputAction, useLastInputDevice} from 'react-native-godot/input';
import {useGodotDrag} from 'react-native-godot/root';
import {color} from 'react-native-godot/values';

global.__godotStyling = {layouts: {}};

function GameStylingApp() {
  const [strength, setStrength] = useState(0.25);
  global.__godotStyling.setStrength = setStrength;
  const record = name => event => {
    global.__godotStyling.layouts[name] = event.nativeEvent.layout;
  };
  return (
    <View style={styles.fill}>
      <View nativeID="themed" godotThemeType="FramedPanel" style={styles.box} />
      <View nativeID="framed" godotStyleBox="res://smoke/tests/game_styling/frame.tres" style={styles.box} />
      <View nativeID="outlined" godotThemeType="FramedPanel" style={[styles.box, styles.outline]} />
      <View nativeID="themed_filtered" godotThemeType="FramedPanel" style={[styles.box, styles.filtered]} />
      <Pressable nativeID="tooltip" tooltip="Restores 50 HP" style={styles.box}>
        <Text nativeID="tooltip_child">Potion</Text>
      </Pressable>
      <Text nativeID="title" godotThemeType="TitleText">Colored</Text>
      <Text onLayout={record('plain')}>Plain</Text>
      <Text godotThemeType="TitleText" onLayout={record('title')}>
        Title <Text>nested</Text>
      </Text>
      <Text godotThemeType="TitleText" style={styles.explicitSize} onLayout={record('sized')}>
        Sized
      </Text>
      <View
        nativeID="shaded"
        godotMaterial="res://smoke/tests/game_styling/tint.tres"
        godotShaderParameters={{strength, tint: color(1, 0, 0)}}
        style={styles.box}>
        <View nativeID="shaded_child" onLayout={record('inner')} style={styles.inner} />
      </View>
      <View
        nativeID="shaded_other"
        godotMaterial="res://smoke/tests/game_styling/tint.tres"
        godotShaderParameters={{strength: 0.75}}
        style={styles.box}
      />
    </View>
  );
}

const styles = StyleSheet.create({
  fill: {flex: 1},
  box: {width: 40, height: 20},
  inner: {width: 10, height: 10, backgroundColor: '#ffffff'},
  outline: {outlineWidth: 2, outlineColor: '#00ff00', outlineStyle: 'solid'},
  explicitSize: {fontSize: 15},
  input: {width: 200, height: 30},
  filtered: {filter: [{brightness: 0.5}]},
  dropBox: {width: 60, height: 60, margin: 20},
  panBox: {width: 200, height: 200},
  minimap: {width: 100, height: 100},
  narrow: {width: 80},
  alignStart: {alignItems: 'flex-start'},
  slider: {width: 200, height: 24},
  verticalSlider: {width: 24, height: 100},
});

AppRegistry.registerComponent('GameStylingApp', () => GameStylingApp);

global.__godotComponents = {layouts: {}, viewportErrors: [], viewportLoads: 0, revealed: 0, videoLoads: 0, videoEnds: 0};

function GameComponentsApp() {
  const [value, setValue] = useState(30);
  const rich = useRef(null);
  const dialogue = useRef(null);
  const [line, setLine] = useState('The first line of the dialogue.');
  const state = global.__godotComponents;
  state.setLine = setLine;
  state.revealDialogue = () => GodotRichTextCommands.reveal(dialogue.current, 1, 0.05);
  state.setValue = setValue;
  state.reveal = () => GodotRichTextCommands.reveal(rich.current, 1, 0.05);
  const record = name => event => {
    state.layouts[name] = event.nativeEvent.layout;
  };
  return (
    <View style={styles.fill}>
      <GodotProgressBar nativeID="bar" value={value} maximumValue={60} fillMode="endToBegin" onLayout={record('bar')} />
      <GodotProgressBar godotThemeType="TallBar" value={1} onLayout={record('themedBar')} />
      <GodotTextureProgressBar
        nativeID="ring"
        value={0.5}
        maximumValue={1}
        step={0.01}
        fillMode="clockwise"
        radialFillDegrees={180}
        underTexture={{uri: 'res://smoke/tests/game_components/under.png'}}
        progressTexture={{uri: 'res://smoke/tests/game_components/fill.png'}}
        style={styles.box}
      />
      <GodotRichText
        ref={rich}
        nativeID="rich"
        bbcode={'[b]Merchant:[/b] Want to see my [smoke]rare[/smoke] [url=shop]wares[/url]? ' + 'Everything is on sale today, traveller. '.repeat(6)}
        customEffects={['res://smoke/tests/game_components/wave_effect.tres']}
        onLayout={record('rich')}
        onRevealFinished={() => {
          state.revealed += 1;
        }}
        onMetaClick={event => {
          state.meta = event.nativeEvent.meta;
        }}
      />
      <GodotRichText bbcode="Short" onLayout={record('richShort')} />
      <View style={styles.alignStart}>
        <GodotRichText bbcode="Effect" onLayout={record('plainWord')} />
        <GodotRichText
          bbcode="[smoke]Effect[/smoke]"
          customEffects={['res://smoke/tests/game_components/wave_effect.tres']}
          onLayout={record('effectWord')}
        />
      </View>
      <GodotRichText ref={dialogue} nativeID="dialogue" bbcode={line} visibleRatio={0} />
      <GodotRichText nativeID="half" bbcode={line} visibleRatio={0.5} />
      <View style={styles.narrow}>
        <GodotRichText bbcode={'Wrapping text. '.repeat(8)} onLayout={record('richNarrow')} />
      </View>
      <GodotRichText bbcode={'Wrapping text. '.repeat(8)} onLayout={record('richWide')} />
      <GodotViewportView
        nativeID="minimap"
        viewport="minimap"
        resizeMode="stretch"
        matchSize
        forwardInput
        style={styles.minimap}
        onLoad={() => {
          state.viewportLoads += 1;
        }}
        onError={event => state.viewportErrors.push(event.nativeEvent.message)}
      />
      <GodotVideo
        nativeID="video"
        source="res://smoke/tests/game_components/clip.ogv"
        autoplay
        style={styles.box}
        onLoad={() => {
          state.videoLoads += 1;
        }}
        onEnd={() => {
          state.videoEnds += 1;
        }}
      />
    </View>
  );
}

AppRegistry.registerComponent('GameComponentsApp', () => GameComponentsApp);

global.__godotSlider = {events: [], layouts: {}};

function GameSliderApp() {
  const state = global.__godotSlider;
  const [value, setValue] = useState(0.25);
  state.setValue = setValue;
  return (
    <View style={styles.fill}>
      <Slider
        nativeID="slider"
        style={styles.slider}
        value={value}
        minimumValue={0}
        maximumValue={1}
        step={0.05}
        upperLimit={0.8}
        onValueChange={next => state.events.push(['change', next])}
        onSlidingStart={next => state.events.push(['start', next])}
        onSlidingComplete={next => state.events.push(['complete', next])}
        onLayout={event => {
          state.layouts.slider = event.nativeEvent.layout;
        }}
      />
      <Slider nativeID="vertical" vertical style={styles.verticalSlider} value={2} minimumValue={0} maximumValue={10} inverted />
      <Slider
        nativeID="skinned"
        style={styles.slider}
        thumbImage={{uri: 'res://smoke/tests/game_components/fill.png'}}
        thumbTintColor="#00ff00"
        minimumTrackTintColor="#ff0000"
      />
      <Slider
        onLayout={event => {
          state.layouts.unsized = event.nativeEvent.layout;
        }}
      />
    </View>
  );
}

AppRegistry.registerComponent('GameSliderApp', () => GameSliderApp);

global.__godotInput = {actions: [], devices: []};

function GameInputApp() {
  const state = global.__godotInput;
  useInputAction('smoke_jump', event => state.actions.push(event));
  const device = useLastInputDevice();
  state.device = device;
  state.GodotInput = GodotInput;
  return <TextInput nativeID="field" style={styles.input} />;
}

AppRegistry.registerComponent('GameInputApp', () => GameInputApp);

global.__godotAudio = {GodotAudio, renders: 0, wav: require('./smoke/tests/game_audio/beep.wav'), ogg: require('./smoke/tests/game_audio/beep.ogg')};

function GameAudioApp() {
  const state = global.__godotAudio;
  state.music = useAudioBus('Master');
  state.renders += 1;
  return <View style={styles.fill} />;
}

AppRegistry.registerComponent('GameAudioApp', () => GameAudioApp);

global.__godotI18n = {setLocale, getLoadedLocales, tr, renders: 0};

function GameI18nApp() {
  const {tr: translate, trN: translatePlural, locale} = useTranslation();
  const state = global.__godotI18n;
  state.locale = locale;
  state.greeting = translate('GREETING');
  state.items = translatePlural('ITEM', 'ITEMS', 2);
  state.renders += 1;
  return <Text>{state.greeting}</Text>;
}

AppRegistry.registerComponent('GameI18nApp', () => GameI18nApp);

global.__godotStorage = {AsyncStorage, createAsyncStorage, native: TurboModuleRegistry.get('RNAsyncStorage')};

function GameStorageApp() {
  return <View style={styles.fill} />;
}

AppRegistry.registerComponent('GameStorageApp', () => GameStorageApp);

global.__godotDrag = {drops: [], ends: [], seen: []};

function GameDragApp() {
  const state = global.__godotDrag;
  const active = useGodotDrag();
  if (active) {
    state.seen.push(active);
  }
  return (
    <View style={styles.fill}>
      <View
        nativeID="source"
        godotDragData={{type: 'item', id: 7}}
        godotDragPreview={{uri: 'res://smoke/tests/game_components/fill.png'}}
        onGodotDragEnd={event => state.ends.push(event.nativeEvent)}
        style={styles.dropBox}>
        <View nativeID="source_child" style={styles.fill} />
      </View>
      <View
        nativeID="target"
        godotDropTypes={['item']}
        onGodotDrop={event => state.drops.push(event.nativeEvent)}
        style={styles.dropBox}>
        <View nativeID="target_child" style={styles.fill} />
      </View>
      <View nativeID="refuses" godotDropTypes={['spell']} style={styles.dropBox} />
    </View>
  );
}

AppRegistry.registerComponent('GameDragApp', () => GameDragApp);

global.__godotPan = {events: []};

function GamePanApp() {
  const state = global.__godotPan;
  const responder = useRef(
    PanResponder.create({
      onStartShouldSetPanResponder: () => true,
      onMoveShouldSetPanResponder: () => true,
      onPanResponderGrant: (_, gesture) => state.events.push(['grant', {...gesture}]),
      onPanResponderMove: (_, gesture) => state.events.push(['move', {...gesture}]),
      onPanResponderRelease: (_, gesture) => state.events.push(['release', {...gesture}]),
      onPanResponderTerminate: (_, gesture) => state.events.push(['terminate', {...gesture}]),
    }),
  ).current;
  return (
    <View style={styles.fill}>
      <View nativeID="pan" {...responder.panHandlers} style={styles.panBox} />
      <View nativeID="pan_drag" {...responder.panHandlers} godotDragData={{type: 'pan'}} style={styles.panBox} />
    </View>
  );
}

AppRegistry.registerComponent('GamePanApp', () => GamePanApp);
