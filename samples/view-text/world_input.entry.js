import React from 'react';
import Slider from '@react-native-community/slider';
import {AppRegistry, Pressable, ScrollView, StyleSheet, Text, TextInput, View} from 'react-native';

const fixture = (global.__godotWorld = {events: [], scrollY: 0, slider: null});
const record = name => () => fixture.events.push(name);

function WorldInputApp() {
  return (
    <View style={styles.fill}>
      <View nativeID="box_none" pointerEvents="box-none" style={styles.overlay}>
        <Pressable nativeID="pressable" onPress={record('press')} style={styles.box} />
        <View nativeID="opaque" style={styles.box} />
        <ScrollView
          nativeID="scroll"
          style={styles.scroll}
          scrollEventThrottle={1}
          onScroll={event => {
            fixture.scrollY = event.nativeEvent.contentOffset.y;
          }}>
          {Array.from({length: 20}, (_, index) => (
            <Text key={index} nativeID={'row_' + index} style={styles.row}>
              Row {index}
            </Text>
          ))}
        </ScrollView>
        <Slider nativeID="slider" style={styles.slider} value={0} onValueChange={value => (fixture.slider = value)} />
        <TextInput nativeID="input" style={styles.input} defaultValue="World" />
        <Pressable nativeID="box_only" pointerEvents="box-only" onPress={record('outer')} style={styles.box}>
          <Pressable nativeID="box_only_child" onPress={record('inner')} style={styles.child} />
        </Pressable>
      </View>
    </View>
  );
}

AppRegistry.registerComponent('GodotWorldInputApp', () => WorldInputApp);

const styles = StyleSheet.create({
  fill: {flex: 1},
  overlay: {position: 'absolute', left: 0, top: 0, right: 0, height: 560, padding: 10, gap: 10},
  box: {width: 120, height: 50, backgroundColor: '#557799'},
  child: {width: 60, height: 30, backgroundColor: '#99bb33'},
  scroll: {width: 200, height: 90, backgroundColor: '#333333'},
  row: {height: 30},
  slider: {width: 200, height: 30},
  input: {width: 200, height: 40},
});
