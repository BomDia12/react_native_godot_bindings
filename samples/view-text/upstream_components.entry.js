import React, {useEffect, useRef, useState} from 'react';
import {
  AppRegistry,
  DeviceEventEmitter,
  ImageBackground,
  KeyboardAvoidingView,
  StyleSheet,
  Text,
  TouchableHighlight,
  TouchableOpacity,
  TouchableWithoutFeedback,
  View,
  VirtualizedList,
  VirtualizedSectionList,
} from 'react-native';

const fixture = (global.__godotUpstream = {
  events: [],
  mounted: new Set(),
  maxMounted: 0,
  showCase: () => {},
  scrollSections: () => {},
  showKeyboard: screenY =>
    DeviceEventEmitter.emit('keyboardDidShow', {
      startCoordinates: {screenX: 0, screenY: 600, width: 800, height: 0},
      endCoordinates: {screenX: 0, screenY, width: 800, height: 600 - screenY},
      duration: 0,
      easing: 'keyboard',
      isEventFromThisApp: true,
    }),
  hideKeyboard: () =>
    DeviceEventEmitter.emit('keyboardDidHide', {
      endCoordinates: {screenX: 0, screenY: 600, width: 800, height: 0},
      duration: 0,
      easing: 'keyboard',
      isEventFromThisApp: true,
    }),
});

const record = name => () => fixture.events.push(name);

function Touchables() {
  return (
    <View style={styles.column}>
      <TouchableOpacity
        nativeID="opacity"
        activeOpacity={0.4}
        delayPressOut={300}
        delayLongPress={400}
        onPressIn={record('opacity:pressIn')}
        onPressOut={record('opacity:pressOut')}
        onPress={record('opacity:press')}
        onLongPress={record('opacity:longPress')}
        style={styles.box}
      />
      <TouchableOpacity
        nativeID="opacity_disabled"
        disabled
        onPressIn={record('disabled:pressIn')}
        onPressOut={record('disabled:pressOut')}
        onPress={record('disabled:press')}
        onLongPress={record('disabled:longPress')}
        style={styles.box}
      />
      <TouchableHighlight
        nativeID="highlight"
        activeOpacity={0.5}
        underlayColor="#ff0000"
        onShowUnderlay={record('highlight:show')}
        onHideUnderlay={record('highlight:hide')}
        onPress={record('highlight:press')}
        style={styles.box}>
        <View nativeID="highlight_child" style={styles.fillBlue} />
      </TouchableHighlight>
      <View style={styles.slopRow}>
        <TouchableWithoutFeedback
          nativeID="without_feedback"
          hitSlop={20}
          pressRetentionOffset={60}
          onPressIn={record('without:pressIn')}
          onPress={record('without:press')}>
          <View style={styles.roundedBox} />
        </TouchableWithoutFeedback>
      </View>
    </View>
  );
}

function Cell({index}) {
  useEffect(() => {
    fixture.mounted.add(index);
    fixture.maxMounted = Math.max(fixture.maxMounted, fixture.mounted.size);
    return () => fixture.mounted.delete(index);
  }, [index]);
  return <Text style={styles.cell}>Row {index}</Text>;
}

const sections = Array.from({length: 6}, (_, section) => ({
  key: 'section_' + section,
  data: [0, 1, 2].map(item => section * 10 + item),
}));

function Lists() {
  const sectionList = useRef(null);
  fixture.scrollSections = sectionIndex =>
    sectionList.current?.scrollToLocation({sectionIndex, itemIndex: 0, animated: false});
  return (
    <View style={styles.row}>
      <VirtualizedList
        nativeID="virtualized"
        style={styles.list}
        windowSize={3}
        initialNumToRender={10}
        maxToRenderPerBatch={10}
        getItem={(_, index) => index}
        getItemCount={() => 1000}
        keyExtractor={item => String(item)}
        renderItem={({item}) => <Cell index={item} />}
      />
      <VirtualizedSectionList
        ref={sectionList}
        nativeID="sections"
        style={styles.list}
        sections={sections}
        getItem={(data, index) => data[index]}
        getItemCount={data => data.length}
        keyExtractor={item => String(item)}
        renderSectionHeader={({section}) => (
          <View nativeID={'header_' + section.key} style={styles.header}>
            <Text>{section.key}</Text>
          </View>
        )}
        renderItem={({item}) => (
          <View nativeID={'item_' + item} style={styles.sectionItem}>
            <Text>Item {item}</Text>
          </View>
        )}
      />
    </View>
  );
}

function Background() {
  return (
    <ImageBackground
      nativeID="background_image"
      source={require('./assets/primary/badge.png')}
      imageStyle={styles.halfOpaque}
      style={styles.background}>
      <View nativeID="background_child" style={styles.backgroundChild} />
    </ImageBackground>
  );
}

function Keyboard() {
  return (
    <View style={styles.row}>
      <KeyboardAvoidingView nativeID="kav_padding" behavior="padding" style={styles.avoiding}>
        <View nativeID="kav_padding_child" style={styles.fillBlue} />
      </KeyboardAvoidingView>
      <KeyboardAvoidingView nativeID="kav_height" behavior="height" style={styles.avoiding}>
        <View style={styles.fillBlue} />
      </KeyboardAvoidingView>
      <KeyboardAvoidingView
        nativeID="kav_position"
        behavior="position"
        style={styles.avoiding}
        contentContainerStyle={styles.avoidingContent}>
        <View nativeID="kav_position_child" style={styles.fillBlue} />
      </KeyboardAvoidingView>
    </View>
  );
}

const cases = {touchables: Touchables, lists: Lists, background: Background, keyboard: Keyboard};

function UpstreamComponentsApp() {
  const [name, setName] = useState('touchables');
  fixture.showCase = setName;
  const Case = cases[name];
  return (
    <View nativeID={'case_' + name} style={styles.fill}>
      <Case />
    </View>
  );
}

AppRegistry.registerComponent('GodotUpstreamComponentsApp', () => UpstreamComponentsApp);

const styles = StyleSheet.create({
  fill: {flex: 1},
  column: {padding: 20, gap: 20},
  row: {flexDirection: 'row'},
  box: {width: 100, height: 60, backgroundColor: '#336699'},
  // Its own rounded clip doesn't cut the hitSlop around it.
  roundedBox: {width: 100, height: 60, borderRadius: 20, overflow: 'hidden', backgroundColor: '#336699'},
  fillBlue: {flex: 1, backgroundColor: '#0000ff'},
  slopRow: {paddingLeft: 100},
  list: {width: 300, height: 300},
  cell: {height: 30},
  header: {height: 40, backgroundColor: '#cccccc'},
  sectionItem: {height: 30},
  background: {width: 200, height: 200},
  halfOpaque: {opacity: 0.5},
  backgroundChild: {width: 50, height: 50, backgroundColor: '#00ff00'},
  avoiding: {width: 200, height: 400},
  avoidingContent: {height: 400},
});
