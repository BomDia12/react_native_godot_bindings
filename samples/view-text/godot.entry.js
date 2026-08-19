// The preamble is imported here only so it lands in Metro's module graph; Metro
// drops a getModulesRunBeforeMainModule entry that nothing else pulls in. What
// actually makes it run *before* InitializeCore is metro.config.godot.js.
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

// ReactNativeRootView calls this after evaluating the bundle, with its root tag.
// fabric: true is what selects the ReactFabric renderer — the one that talks to
// global.nativeFabricUIManager. Without it RN falls back to the legacy Paper renderer
// and calls UIManager.createView, which does not exist here.
global.__godotRunApplication = rootTag =>
  AppRegistry.runApplication('GodotApp', {
    rootTag,
    initialProps: {},
    fabric: true,
  });
