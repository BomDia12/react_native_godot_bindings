import React, {useState, useRef, useEffect} from 'react';
import {AppRegistry, View, Text, Image, ScrollView, FlatList, SectionList, TextInput, Button, Pressable, Modal, Switch, ActivityIndicator} from 'react-native';
import GodotWindow from 'react-native-godot/GodotWindow';
import {fontFamily} from 'react-native-godot/fonts';
import '../../js/godot/bootstrap-finalize';

const itemIcon = require('./assets/item.png');
const importedFont = fontFamily(require('./assets/OpenSans.woff2'));
const rows = Array.from({length: 100}, (_, index) => ({id: String(index), title: 'Item ' + index}));
const sections = [{title: 'Equipment', data: rows.slice(0, 8)}, {title: 'Supplies', data: rows.slice(8, 16)}];
const fixture = {enemySetters: {}, refs: {}};
const state = global.__phase6a = {events: [], enemies: {}, renderedItems: []};
const record = name => state.events.push(name);
const dataIcon = {uri: 'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABgAAAAYCAAAAADFHGIkAAAAAmJLR0QA/4ePzL8AAAAHdElNRQfqCgECJyKFJspvAAAAJXRFWHRkYXRlOmNyZWF0ZQAyMDI2LTEwLTAxVDAyOjM5OjE3KzAwOjAwucRQNQAAACV0RVh0ZGF0ZTptb2RpZnkAMjAyNi0xMC0wMVQwMjozOToxNyswMDowMMiZ6IkAAAAodEVYdGRhdGU6dGltZXN0YW1wADIwMjYtMTAtMDFUMDI6Mzk6MzQrMDA6MDDsQdS2AAABbUlEQVQoz22SsW4TQRCGv39u13cRZ8HJRhShgT5NpEQUlKnCC/AI5AnyGunIi4SKHilI9DSWCLiIZMsuYnH23e1Q+BwsxDY7mp35Z/T9qzNc/OcEfwwlSAAuXIG+3tqmI4shuXAg9OnmoXpVaTFdFDEhIOAIW43en4wH3sy/3syeJIDsNcgeji9P8nbT+PDozfRHnrR9yFbHlwcrl6SuLt9O7nIHk6wZfdA6CIGytV+MNwJzVJ+/qDMHB7D183e1wKB9elqbQ7+4/T6tWjDUvBy3gr4DtaPDjTApVdH3YXiskjB2Gv8QM9wWjf7mhZqFOeYep7OwGwDucfZr4JgIy9uDTjv6SsXtMoBBKj7dF13fo664vykcDIjzj+TdVqjNuZ4PfMvK87vJ0Tgld8VyfvWtTD1dPP/5pavKYsDs8/WkTDjSWW9U/eywYjFdFjE5cvUOpjBcf+/I4tDT1vXwuL4Vwr1DyMH3fonvXc4fihicbbO3bcMAAAAASUVORK5CYII='};

function EnemyPanel({rootTag}) {
  const [health, setHealth] = useState(100);
  useEffect(() => { fixture.enemySetters[rootTag] = setHealth; return () => delete fixture.enemySetters[rootTag]; }, [rootTag]);
  state.enemies[rootTag] = health;
  return <View style={{width: 180, height: 16, backgroundColor: '#333333'}}><View testID="health" style={{width: health + '%', height: 16, backgroundColor: '#cc3333'}} /></View>;
}
function Inventory() {
  const [enabled, setEnabled] = useState(false);
  const [switchPointerEvents, setSwitchPointerEvents] = useState('auto');
  const [modal, setModal] = useState(false);
  const [nested, setNested] = useState(false);
  const [windowVisible, setWindowVisible] = useState(false);
  const [windowModal, setWindowModal] = useState(false);
  const [controlled, setControlled] = useState('Controlled');
  const [multiline, setMultiline] = useState(false);
  const list = useRef(null);
  const editor = useRef(null);
  const [version, setVersion] = useState(0);
  fixture.refs.list = list;
  fixture.refs.editor = editor;
  fixture.showModal = setModal;
  fixture.showNested = setNested;
  fixture.showWindow = setWindowVisible;
  fixture.showWindowModal = setWindowModal;
  fixture.setMultiline = setMultiline;
  fixture.rerender = () => setVersion(value => value + 1);
  fixture.scrollList = index => list.current?.scrollToIndex({index, animated: false});
  fixture.setControlled = setControlled;
  fixture.setSwitchPointerEvents = setSwitchPointerEvents;
  state.controlled = controlled;
  const renderItem = ({item}) => {
    state.renderedItems.push(item.id);
    return <View style={{height: 32, flexDirection: 'row'}}><Image source={itemIcon} style={{width: 24, height: 24}} /><Text>{item.title}</Text></View>;
  };
  return <View style={{flex: 1, padding: 12, backgroundColor: '#18202b'}}>
    <Text testID="imported-font" style={{fontFamily: importedFont, fontSize: 22, color: '#eeeeee'}}>Inventory and settings</Text>
    <View style={{flexDirection: 'row', height: 44, alignItems: 'center'}}>
      <Image testID="asset-image" source={itemIcon} style={{width: 32, height: 32}} onLoad={() => record('asset-load')} />
      <Image testID="data-image" source={dataIcon} style={{width: 32, height: 32}} onLoad={() => record('data-load')} onError={() => record('data-error')} />
      <View pointerEvents={switchPointerEvents}><Switch testID="settings-switch" value={enabled} onValueChange={value => {setEnabled(value); record('switch-' + value);}} /></View>
      <ActivityIndicator animating={enabled} hidesWhenStopped={false} />
      <Button title="Dialog" onPress={() => setModal(true)} />
      <Button title="Window" onPress={() => setWindowVisible(true)} />
      <Button testID="refresh-button" title="Refresh" onPress={() => {record('refresh'); setVersion(value => value + 1);}} />
    </View>
    <View style={{height: 240, flexDirection: 'row'}}>
      <FlatList testID="inventory-list" ref={list} data={rows} renderItem={renderItem} keyExtractor={item => item.id}
        initialNumToRender={5} maxToRenderPerBatch={5} windowSize={3}
        getItemLayout={(_, index) => ({length: 32, offset: index * 32, index})}
        onViewableItemsChanged={({viewableItems}) => {state.viewable = viewableItems.map(entry => entry.item.id);}}
        style={{width: 320, height: 220}} />
      <SectionList testID="section-list" sections={sections} keyExtractor={item => item.id} renderItem={renderItem}
        stickySectionHeadersEnabled renderSectionHeader={({section}) => <Text testID={"section-header-" + section.title} style={{height: 28, backgroundColor: '#334466'}}>{section.title}</Text>}
        style={{width: 320, height: 220}} />
    </View>
    <TextInput testID="inventory-editor" ref={editor} defaultValue="Uncontrolled 😀" multiline={multiline} style={{height: 48, color: '#ffffff'}}
      onChangeText={text => {state.uncontrolled = text; record('edit');}} onSubmitEditing={() => record('submit')} />
    <TextInput testID="controlled-editor" autoFocus value={controlled} onChangeText={setControlled} style={{height: 36, color: '#ffffff'}} />
    <ScrollView testID="native-scroll" style={{height: 120}} showsVerticalScrollIndicator>
      {rows.slice(0, 10).map(item => <Text key={item.id} style={{height: 32}}>{item.title}</Text>)}
    </ScrollView>
    <Pressable testID="context-target" godotContextMenu={[{id: 'inspect', label: 'Inspect'}, {id: 'use', label: 'Use'}]}
      onGodotContextMenuAction={event => record('menu-' + event.nativeEvent.id)} onRightClick={() => record('right')} onMiddleClick={() => record('middle')}
      onPress={() => record('press')} style={{height: 28, backgroundColor: '#557755'}}><Text>Inspect an item</Text></Pressable>
    <Text selectable style={{height: 30, color: '#ffffff'}}>Native <Text style={{fontWeight: 'bold'}}>selection 😀</Text> and bidi العربية</Text>
    <Text style={{height: 20}}>Revision {version}</Text>
    <Modal visible={modal} transparent onShow={() => record('modal-show')} onDismiss={() => record('modal-dismiss')} onRequestClose={() => {record('modal-close'); setModal(false);}}>
      <View style={{margin: 100, padding: 16, backgroundColor: '#334466'}}><Text>Settings dialog</Text><Button title="Nested" onPress={() => setNested(true)} /><Button title="Close" onPress={() => setModal(false)} />
        <Modal visible={nested} transparent onShow={() => record('nested-show')} onRequestClose={() => setNested(false)}><View style={{margin: 150, backgroundColor: '#556677'}}><Text>Nested dialog</Text><Button title="Close nested" onPress={() => setNested(false)} /></View></Modal>
      </View>
    </Modal>
    <GodotWindow visible={windowVisible} title="Inventory details" width={480} height={320} onRequestClose={() => setWindowVisible(false)}>
      <View style={{flex: 1, padding: 16, backgroundColor: '#445566'}}><Text>Native Godot window</Text><TextInput testID="window-editor" defaultValue="Window editor" onChangeText={text => {state.windowText = text;}} style={{height: 40}} />
        <Modal visible={windowModal} transparent onShow={() => record("window-modal-show")} onRequestClose={() => setWindowModal(false)}><View testID="window-modal-content" style={{flex: 1, backgroundColor: "#334466"}}><Text>Window-scoped dialog</Text></View></Modal>
      </View>
    </GodotWindow>
  </View>;
}
AppRegistry.registerComponent('EnemyPanel', () => EnemyPanel);
AppRegistry.registerComponent('Inventory', () => Inventory);
global.__godotRunApplication = (key, rootTag) => AppRegistry.runApplication(key, {rootTag, initialProps: {rootTag}, fabric: true});
global.__godotStopApplication = rootTag => global.RN$stopSurface(rootTag);

global.__phase6aAction = (action, value) => {
  if (action === 'switch-pointer-events') { fixture.setSwitchPointerEvents(value); }
  else if (action === 'rerender') { fixture.rerender(); }
  else if (action === 'scroll') { fixture.scrollList(value); }
  else if (action === 'modal') { fixture.showModal(value); }
  else if (action === 'nested') { fixture.showNested(value); }
  else if (action === 'window-modal') { fixture.showWindowModal(value); }
  else if (action === 'window') { fixture.showWindow(value); }
  else if (action === 'mode') { fixture.setMultiline(value); }
  else if (action === 'controlled') { fixture.setControlled(value); }
  else if (action === 'clear') { fixture.refs.editor.current.clear(); }
  else if (action === 'focus') { fixture.refs.editor.current.focus(); }
  else if (action === 'enemy') { fixture.enemySetters[value.rootTag]?.(value.health); }
  else if (action === 'image-size') { Image.getSize(Image.resolveAssetSource(itemIcon).uri).then(size => { state.imageSize = size; }); }
};
