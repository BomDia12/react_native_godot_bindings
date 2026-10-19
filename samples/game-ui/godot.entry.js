import React, {useState, useRef, useEffect} from 'react';
import {AccessibilityInfo, Alert, Animated, AppRegistry, Easing, LayoutAnimation, I18nManager, findNodeHandle, View, Text, Image, FlatList, SectionList, ScrollView, TextInput, Button, Pressable, Switch, ActivityIndicator, Modal, Appearance, useWindowDimensions} from 'react-native';
import {GodotAppRegistry, useGodotAlert} from 'react-native-godot/app-registry';
import {useGodotScene, call, callAsync} from 'react-native-godot/scene';
import GodotWindow from 'react-native-godot/GodotWindow';
import {fontFamily} from 'react-native-godot/fonts';
import 'react-native-godot/setup';

import {runHTTPChecks, runSocketChecks, runLowFPSDownload} from './game/network';
const icon = require('./assets/item.png');
const font = fontFamily(require('./assets/OpenSans.woff2'));
const arabicFont = fontFamily(require('./assets/NotoSansArabic.ttf'));
const observations = global.__game = {enemies: {}, inventory: null, sequences: {}, events: [], rendered: [], network: []};
const actions = new Map();
let savedAlert;
const record = name => observations.events.push(name);
function useBinding(rootTag) {
  const binding = useGodotScene(rootTag);
  useEffect(() => {
    if (binding) {
      observations.sequences[rootTag] = binding.sequence;
    }
  }, [binding, rootTag]);
  return binding;
}
function EnemyStatus({rootTag}) {
  const binding = useBinding(rootTag);
  const alert = useGodotAlert();
  useEffect(() => {
    const invoke = () => alert('Enemy alert', 'Root policy', [{text: 'OK', onPress: () => record('enemy-alert-button')}]).catch(error => {observations.alertError = error.code;});
    actions.set(rootTag, {testAlert: invoke, saveAlert: () => {savedAlert = invoke;}});
    return () => {delete observations.enemies[rootTag]; actions.delete(rootTag);};
  }, [rootTag, alert]);
  if (!binding) { return null; }
  const enemy = binding.value;
  observations.enemies[rootTag] = enemy;
  return <View style={{height: 36}}>
    <View style={{height: 16, backgroundColor: '#333333'}}>
      <View testID="health" style={{width: enemy.health / enemy.maxHealth * 100 + '%', height: 16, backgroundColor: '#cc3333'}} />
    </View>
    <Text style={{color: '#eeeeee', fontSize: 12}}>{enemy.name} {enemy.health}/{enemy.maxHealth}{enemy.lastDamage ? ' -' + enemy.lastDamage : ''}</Text>
  </View>;
}
function GameInventory({rootTag}) {
  const binding = useBinding(rootTag);
  const alert = useGodotAlert();
  const [dark, setDark] = useState(false);
  const [modal, setModal] = useState(false);
  const [windowVisible, setWindowVisible] = useState(false);
  const [remote, setRemote] = useState(null);
  const [endpoint, setEndpoint] = useState('');
  const [busy, setBusy] = useState(false);
  const [controlled, setControlled] = useState('Controlled');
  const list = useRef(null);
  const editor = useRef(null);
  const dimensions = useWindowDimensions();
  observations.dimensions = dimensions;
  observations.controlled = controlled;
  const command = (name, args = []) => binding && call(binding.rootTag, binding.handle, name, args);
  const queued = (name, args = []) => binding && callAsync(binding.rootTag, binding.handle, name, args);
  const refresh = async base => {
    setBusy(true);
    try {
      const response = await fetch(base + '/inventory');
      if (!response.ok) { throw new Error('Inventory HTTP ' + response.status); }
      const items = await response.json();
      await queued('importItems', [items]);
      setRemote({uri: base + '/icon.png'});
      record('refresh');
    } finally { setBusy(false); }
  };
  useEffect(() => {
    actions.set(rootTag, {
      command, queued, refresh,
      testAlert: (title = 'Inventory alert') => alert(title, 'Local policy', [{text: 'OK', onPress: () => record('inventory-alert-button')}], {cancelable: true}).then(() => record('alert-settled')).catch(error => {observations.alertError = error.code;}),
      ambientAlert: () => Alert.alert('Unattributed', 'Godot handler', [{text: 'OK', onPress: () => record('ambient-alert-button')}]),
      staleAlert: () => savedAlert?.(),
      lowFPSDownload: base => runLowFPSDownload(base).then(() => {observations.lowFPSDownload = true;}).catch(error => {observations.networkError = error.message;}),
      httpChecks: (base, https) => runHTTPChecks(base, https).then(evidence => {observations.httpEvidence = evidence;}).catch(error => {observations.networkError = error.message;}),
      socketChecks: url => runSocketChecks(url, update => queued('networkUpdate', [update])).then(evidence => {observations.socketEvidence = evidence;}).catch(error => {observations.networkError = error.message;}),
      alert: (...args) => alert(...args), modal: setModal, window: setWindowVisible,
      accessibility: () => Promise.all([AccessibilityInfo.isScreenReaderEnabled(), AccessibilityInfo.isReduceMotionEnabled()]).then(([screenReader, reduceMotion]) => {
        AccessibilityInfo.announceForAccessibility('Inventory ready');
        AccessibilityInfo.setAccessibilityFocus(findNodeHandle(editor.current));
        observations.accessibility = {screenReader, reduceMotion};
      }),
      forceRTL: value => { I18nManager.forceRTL(value); observations.isRTL = I18nManager.isRTL; },
      scroll: index => list.current?.scrollToIndex({index, animated: false}),
      controlled: setControlled, clear: () => editor.current?.clear(),
      scheme: value => { Appearance.setColorScheme(value); setDark(value === 'dark'); },
      socket: url => new Promise((resolve, reject) => {
        const socket = new WebSocket(url, ['fixture']);
        observations.socket = {opened: false, messages: [], closes: []};
        socket.onopen = () => { observations.socket.opened = true; socket.send('updates'); };
        socket.onmessage = async event => {
          observations.socket.messages.push(event.data);
          if (typeof event.data === 'string') {
            await queued('networkUpdate', [JSON.parse(event.data)]);
            socket.close(1000, 'done');
          }
        };
        socket.onerror = reject;
        socket.onclose = event => { observations.socket.closes.push({code: event.code, reason: event.reason, wasClean: event.wasClean}); resolve(); };
      }),
    });
    return () => actions.delete(rootTag);
  }, [binding, rootTag]);
  if (!binding) { return null; }
  const inventory = binding.value;
  observations.inventory = inventory;
  const renderItem = ({item}) => {
    observations.rendered.push(item.id);
    return <Pressable onPress={() => command('use', [item.id])} style={{height: 32, flexDirection: 'row'}}>
      <Image source={icon} style={{width: 24, height: 24}} /><Text style={{color: '#dddddd'}}>{item.title} ×{item.quantity}{item.equipped ? ' equipped' : ''}</Text>
    </Pressable>;
  };
  return <View style={{flex: 1, padding: 12, backgroundColor: dark ? '#111122' : '#18202b'}}>
    <Text testID="imported-font" style={{fontFamily: font, fontSize: 22, color: '#eeeeee'}}>{inventory.title}</Text>
    <Text testID="bidi-text" style={{color: '#eeeeee', alignSelf: 'flex-start'}}>Market <Text onPress={() => record('arabic-press')} style={{fontFamily: arabicFont}}>سوق</Text></Text>
    <View style={{flexDirection: 'row', height: 44, alignItems: 'center'}}>
      <Image testID="asset-image" source={icon} style={{width: 32, height: 32}} onLoad={() => record('asset-load')} />
      {remote && <Image testID="remote-image" source={remote} style={{width: 32, height: 32}} onLoad={() => record('remote-load')} onError={event => {observations.networkError = event.nativeEvent.error;}} />}
      <Switch testID="settings-switch" value={dark} onValueChange={value => {setDark(value); Appearance.setColorScheme(value ? 'dark' : 'light');}} />
      <ActivityIndicator animating={busy} hidesWhenStopped={false} />
      <Button title="Use potion" onPress={() => command('use', ['potion'])} />
      <Button title="Dialog" onPress={() => setModal(true)} />
      <Button title="Window" onPress={() => setWindowVisible(true)} />
      <Button title="Alert" onPress={() => alert('Inventory', 'Godot owns your items.').catch(error => record(error.code))} />
      <Button testID="refresh-button" title="Refresh" disabled={!endpoint} onPress={() => refresh(endpoint).catch(error => record(error.message))} />
    </View>
    <TextInput testID="network-endpoint" placeholder="Optional local HTTP endpoint" defaultValue="" onChangeText={setEndpoint} style={{height: 32, color: '#ffffff'}} />
    <View style={{height: 240, flexDirection: 'row'}}>
      <FlatList testID="inventory-list" ref={list} data={inventory.items} renderItem={renderItem} keyExtractor={item => item.id}
        initialNumToRender={5} maxToRenderPerBatch={5} windowSize={3}
        getItemLayout={(_, index) => ({length: 32, offset: index * 32, index})}
        onViewableItemsChanged={({viewableItems}) => {observations.viewable = viewableItems.map(entry => entry.item.id);}}
        style={{width: 420, height: 220}} />
      <SectionList testID="section-list" sections={[{title: 'Equipment', data: inventory.items.slice(0, 8)}, {title: 'Supplies', data: inventory.items.slice(8, 16)}]}
        renderItem={renderItem} keyExtractor={item => item.id} stickySectionHeadersEnabled
        renderSectionHeader={({section}) => <Text style={{height: 28, color: '#ffffff'}}>{section.title}</Text>}
        style={{width: 360, height: 220}} />
    </View>
    <TextInput testID="inventory-editor" ref={editor} defaultValue={inventory.title} style={{height: 40, color: '#ffffff'}}
      onChangeText={text => {observations.edited = text;}} onSubmitEditing={event => command('rename', [event.nativeEvent.text])} />
    <TextInput testID="controlled-editor" value={controlled} onChangeText={setControlled} style={{height: 36, color: '#ffffff'}} />
    <ScrollView testID="native-scroll" style={{height: 100}}>{inventory.items.slice(0, 10).map(item => <Text key={item.id} style={{height: 32, color: '#ffffff'}}>{item.title}</Text>)}</ScrollView>
    <Text testID="scaled-text" style={{fontSize: 20, lineHeight: 24, color: '#ffffff'}}>Scaled <Text maxFontSizeMultiplier={1.5}>capped</Text> <Text allowFontScaling={false}>fixed</Text></Text>
    <Text style={{color: '#eeeeee'}}>Godot revision {inventory.revision}</Text>
    <Modal visible={modal} transparent onShow={() => record('modal-show')} onDismiss={() => record('modal-dismiss')} onRequestClose={() => setModal(false)}>
      <View style={{margin: 100, padding: 16, backgroundColor: '#334466'}}><Text>Godot inventory settings</Text><Button title="Close" onPress={() => setModal(false)} /></View>
    </Modal>
    <GodotWindow visible={windowVisible} title="Inventory details" width={480} height={320} onRequestClose={() => setWindowVisible(false)}>
      <View style={{flex: 1, padding: 16}}><Text>Godot inventory revision {inventory.revision}</Text><TextInput defaultValue="Native window" style={{height: 40}} /></View>
    </GodotWindow>
  </View>;
}
function SceneProbe({rootTag}) {
  const binding = useBinding(rootTag);
  global.__probe = binding ? {ready: true, value: binding.value} : {ready: false};
  return <Text>{binding?.value.label ?? 'Waiting'}</Text>;
}
// Drives LayoutAnimation and Animated from the smoke tests, which read the
// mounted Godot controls and the values recorded in global.__animation.
function AnimationProbe() {
  const [boxes, setBoxes] = useState(['first', 'second']);
  const [wide, setWide] = useState(false);
  const jsValue = useRef(new Animated.Value(0)).current;
  const nativeValue = useRef(new Animated.Value(0)).current;
  const scroll = useRef(new Animated.Value(0)).current;
  const decaying = useRef(new Animated.Value(0)).current;
  const probe = global.__animation ??= {layout: [], results: {}, scrolled: 0};
  const run = (name, animation) => new Promise(resolve => animation.start(result => {
    probe.results[name] = {finished: result.finished};
    resolve();
  }));
  const value = (start = 0) => new Animated.Value(start);
  const read = animated => animated.__getValue();
  useEffect(() => {
    probe.layout = [];
    probe.layoutStep = step => {
      LayoutAnimation.configureNext({...LayoutAnimation.Presets.easeInEaseOut, duration: 600}, () => probe.layout.push(step));
      if (step === 'update') { setWide(true); }
      if (step === 'create') { setBoxes(current => [...current, 'third']); }
      if (step === 'delete') { setBoxes(current => current.filter(box => box !== 'first')); }
    };
    probe.conformance = async () => {
      const curves = {linear: Easing.linear, quad: Easing.quad, cubic: Easing.cubic, sin: Easing.sin, circle: Easing.circle, exp: Easing.exp, bounce: Easing.bounce, back: Easing.back(1.5), elastic: Easing.elastic(1), bezier: Easing.bezier(0.25, 0.1, 0.25, 1), inOut: Easing.inOut(Easing.quad), out: Easing.out(Easing.quad), poly: Easing.poly(3), step0: Easing.step0, step1: Easing.step1};
      const ends = {};
      for (const [name, easing] of Object.entries(curves)) {
        const animated = value();
        await run('timing-' + name, Animated.timing(animated, {toValue: 1, duration: 50, easing, useNativeDriver: false}));
        ends['timing-' + name] = read(animated);
      }
      const tension = value();
      await run('spring-tension', Animated.spring(tension, {toValue: 1, tension: 40, friction: 7, useNativeDriver: false}));
      ends['spring-tension'] = read(tension);
      const stiffness = value();
      await run('spring-stiffness', Animated.spring(stiffness, {toValue: 1, stiffness: 200, damping: 20, mass: 1, useNativeDriver: false}));
      ends['spring-stiffness'] = read(stiffness);
      // Upstream's C++ requestAnimationFrame is setTimeout(0), so a JS-driven
      // decay would end on its first frame; the native driver ticks per frame.
      await run('decay', Animated.decay(decaying, {velocity: 0.5, deceleration: 0.995, useNativeDriver: true}));
      const a = value(), b = value(), c = value();
      await run('sequence', Animated.sequence([Animated.timing(a, {toValue: 1, duration: 30, useNativeDriver: false}), Animated.delay(30), Animated.timing(b, {toValue: 2, duration: 30, useNativeDriver: false})]));
      await run('parallel', Animated.parallel([Animated.timing(c, {toValue: 3, duration: 30, useNativeDriver: false}), Animated.timing(a, {toValue: 4, duration: 30, useNativeDriver: false})]));
      const staggered = [value(), value(), value()];
      await run('stagger', Animated.stagger(20, staggered.map(item => Animated.timing(item, {toValue: 1, duration: 20, useNativeDriver: false}))));
      const looped = value();
      await run('loop', Animated.loop(Animated.timing(looped, {toValue: 1, duration: 20, useNativeDriver: false}), {iterations: 3}));
      Object.assign(ends, {sequence: read(b), parallel: read(c) + read(a), stagger: staggered.reduce((sum, item) => sum + read(item), 0), loop: read(looped)});
      const stopped = value();
      const stopping = run('stop', Animated.timing(stopped, {toValue: 1, duration: 1000, useNativeDriver: false}));
      setTimeout(() => stopped.stopAnimation(), 50);
      await stopping;
      ends.stop = read(stopped) < 1 ? 1 : 0;
      const source = value(2);
      ends.extend = read(source.interpolate({inputRange: [0, 1], outputRange: [0, 10], extrapolate: 'extend'}));
      ends.clamp = read(source.interpolate({inputRange: [0, 1], outputRange: [0, 10], extrapolate: 'clamp'}));
      ends.identity = read(source.interpolate({inputRange: [0, 1], outputRange: [0, 10], extrapolate: 'identity'}));
      const offset = value();
      const clamped = Animated.diffClamp(offset, 0, 50);
      clamped.__attach();
      offset.setValue(80);
      read(clamped);
      offset.setValue(30);
      ends.diffClamp = read(clamped);
      probe.ends = ends;
      return true;
    };
    probe.drivers = () => Promise.all([
      run('js-driver', Animated.timing(jsValue, {toValue: 1, duration: 400, easing: Easing.linear, useNativeDriver: false})),
      run('native-driver', Animated.timing(nativeValue, {toValue: 1, duration: 400, easing: Easing.linear, useNativeDriver: true})),
    ]);
  }, []);
  const animatedStyle = driven => ({
    width: 40, height: 40, margin: 4,
    opacity: driven.interpolate({inputRange: [0, 1], outputRange: [1, 0.5]}),
    transform: [{translateX: driven.interpolate({inputRange: [0, 1], outputRange: [0, 30]})}],
  });
  return <View style={{flex: 1, backgroundColor: '#18202b'}}>
    <View style={{flexDirection: 'row', height: 48}}>
      {boxes.map(box => <View key={box} testID={'layout-' + box} style={{width: wide ? 120 : 40, height: 40, margin: 4, backgroundColor: '#edae49'}} />)}
    </View>
    <Animated.View testID="js-driven" style={[animatedStyle(jsValue), {backgroundColor: jsValue.interpolate({inputRange: [0, 1], outputRange: ['#ff0000', '#0000ff']})}]} />
    <Animated.View testID="native-driven" style={[animatedStyle(nativeValue), {backgroundColor: '#00ff00'}]} />
    <Animated.View testID="native-decay" style={{width: 20, height: 20, backgroundColor: '#d1495b', transform: [{translateX: decaying}]}} />
    <Animated.Text testID="js-text" style={{color: jsValue.interpolate({inputRange: [0, 1], outputRange: ['#ff0000', '#0000ff']})}}>Animated text</Animated.Text>
    <Animated.ScrollView testID="animated-scroll" style={{height: 60}} scrollEventThrottle={16}
      onScroll={Animated.event([{nativeEvent: {contentOffset: {y: scroll}}}], {useNativeDriver: false, listener: event => { probe.scrolled = event.nativeEvent.contentOffset.y; }})}>
      <View style={{height: 400, backgroundColor: '#30638e'}} />
    </Animated.ScrollView>
  </View>;
}
GodotAppRegistry.registerComponent('AnimationProbe', () => AnimationProbe);
GodotAppRegistry.registerComponent('SceneProbe', () => SceneProbe);
GodotAppRegistry.registerComponent('EnemyStatus', () => EnemyStatus);
GodotAppRegistry.registerComponent('GameInventory', () => GameInventory, {alerts: true});
GodotAppRegistry.registerComponent('CustomInventory', () => GameInventory, {alerts: payload => {
  record('custom-presenter');
  return payload.title === 'Decline' ? {handled: false} : {buttonId: 0, dismissed: false};
}});
global.__gameAction = (rootTag, name, value) => {actions.get(rootTag)?.[name](...(Array.isArray(value) ? value : [value]));};
