import React, {useState, useRef, useEffect} from 'react';
import {AppRegistry, View, Text, Image, FlatList, SectionList, ScrollView, TextInput, Button, Pressable, Switch, ActivityIndicator, Modal, Appearance, useWindowDimensions} from 'react-native';
import {GodotAppRegistry, useGodotAlert} from 'react-native-godot/app-registry';
import {useGodotScene, call, callAsync} from 'react-native-godot/scene';
import GodotWindow from 'react-native-godot/GodotWindow';
import {fontFamily} from 'react-native-godot/fonts';
import '../../js/godot/bootstrap-finalize';

import {runHTTPChecks, runSocketChecks, runLowFPSDownload} from './game/network';
const icon = require('./assets/item.png');
const font = fontFamily(require('./assets/OpenSans.woff2'));
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
  const command = (name, args = []) => binding && call(binding.session, binding.handle, name, args);
  const queued = (name, args = []) => binding && callAsync(binding.session, binding.handle, name, args);
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
      ambientAlert: () => global.__godotScheduler.withoutOrigin(() => require('react-native/Libraries/Alert/Alert').default.alert('Unattributed', 'Godot handler', [{text: 'OK', onPress: () => record('ambient-alert-button')}])),
      staleAlert: () => savedAlert?.(),
      lowFPSDownload: base => runLowFPSDownload(base).then(() => {observations.lowFPSDownload = true;}).catch(error => {observations.networkError = error.message;}),
      httpChecks: (base, https) => runHTTPChecks(base, https).then(evidence => {observations.httpEvidence = evidence;}).catch(error => {observations.networkError = error.message;}),
      socketChecks: url => runSocketChecks(url, update => queued('networkUpdate', [update])).then(evidence => {observations.socketEvidence = evidence;}).catch(error => {observations.networkError = error.message;}),
      alert: (...args) => alert(...args), modal: setModal, window: setWindowVisible,
      scroll: index => list.current?.scrollToIndex({index, animated: false}),
      controlled: setControlled, clear: () => editor.current?.clear(),
      scheme: value => { Appearance.setColorScheme(value); setDark(value === 'dark'); },
      fontScale: value => global.__godotNativeModules.get('GodotServices').setFontScale(value),
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
GodotAppRegistry.registerComponent('SceneProbe', () => SceneProbe);
GodotAppRegistry.registerComponent('EnemyStatus', () => EnemyStatus);
GodotAppRegistry.registerComponent('GameInventory', () => GameInventory, {alerts: true});
GodotAppRegistry.registerComponent('CustomInventory', () => GameInventory, {alerts: payload => {
  record('custom-presenter');
  return payload.title === 'Decline' ? {handled: false} : {buttonId: 0, dismissed: false};
}});
global.__godotRunApplication = (key, rootTag) => AppRegistry.runApplication(key, {rootTag, initialProps: {rootTag}, fabric: true});
global.__godotStopApplication = rootTag => global.RN$stopSurface(rootTag);
global.__gameAction = (rootTag, name, value) => {actions.get(rootTag)?.[name](...(Array.isArray(value) ? value : [value]));};
