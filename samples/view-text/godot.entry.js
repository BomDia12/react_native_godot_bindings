// Metro config schedules this import before InitializeCore.
import './godot.preamble';

import React from 'react';
import {AppRegistry, StyleSheet, Text, View} from 'react-native';

const styles = StyleSheet.create({
  container: {
    flex: 1,
    padding: 16,
    backgroundColor: '#123456',
    opacity: 0.8,
  },
  sentinel: {
    color: '#00ff00',
    fontSize: 20,
  },
});

const App = () => (
  <View style={styles.container}>
    <Text style={styles.sentinel}>Baseline commit OK</Text>
  </View>
);

AppRegistry.registerComponent('GodotApp', () => App);

// ReactNativeRootView calls this after evaluating the bundle.
global.__godotRunApplication = rootTag =>
  AppRegistry.runApplication('GodotApp', {
    rootTag,
    initialProps: {},
    fabric: true,
  });
