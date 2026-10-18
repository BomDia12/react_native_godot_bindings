import React, {useEffect, useState} from 'react';
import {AppRegistry, BackHandler, Image, Platform, StyleSheet, Text, View} from 'react-native';
import processColor from 'react-native/Libraries/StyleSheet/processColor';
import resolveAssetSource from 'react-native/Libraries/Image/resolveAssetSource';

const state = {
  os: Platform.OS,
  selected: Platform.select({godot: 'godot', native: 'native', default: 'default'}),
  explicitUndefinedPreserved:
    Platform.select({godot: undefined, native: 'wrong'}) === undefined,
  version: Platform.Version,
  constants: Platform.constants,
  microtasks: [],
  unsupportedCode: null,
  backListenerRemovable: false,
};
global.__godotPlatformState = state;

Promise.resolve().then(() => state.microtasks.push('promise'));
queueMicrotask(() => state.microtasks.push('native'));
state.microtasks.push('sync');
state.backListenerRemovable = typeof BackHandler.addEventListener('hardwareBackPress', () => true).remove === 'function';
try {
  BackHandler.exitApp();
} catch (error) {
  state.unsupportedCode = error.code;
}

function PlatformApp() {
  const [removed, setRemoved] = useState(false);
  useEffect(() => {
    setRemoved(true);
  }, []);
  return (
    <View style={[styles.panel, removed && styles.removed]}>
      <Text style={styles.text}>{`Platform ${Platform.OS}`}</Text>
    </View>
  );
}

const primary = resolveAssetSource(require('./assets/primary/badge.png'));
const secondary = resolveAssetSource(require('./assets/secondary/badge.png'));
const explicitUser = resolveAssetSource({
  uri: 'user://rn-godot-asset-smoke.txt',
});
const missing = resolveAssetSource({uri: 'res://missing-phase-5-asset.png'});
global.__godotAssetState = {
  primary,
  secondary,
  explicitUser,
  missing,
  invalidLocalSourcesRejected: 0,
};
for (const uri of [
  '/tmp/asset.png',
  'file://asset.png',
  'res://../asset.png',
  'res://asset\\name.png',
  'res://asset\u0000name.png',
]) {
  Image.getSize(uri).catch(() => {
    global.__godotAssetState.invalidLocalSourcesRejected += 1;
  });
}

function AssetsApp() {
  return (
    <View style={styles.panel}>
      <Text style={styles.text}>{`Asset ${primary.scale} ${primary.width}x${primary.height}`}</Text>
    </View>
  );
}

const styles = StyleSheet.create({
  panel: {width: 100, height: 40, backgroundColor: '#336699'},
  removed: {backgroundColor: null},
  text: {color: processColor('rgba(255, 128, 0, 0.5)'), fontSize: 10},
});

AppRegistry.registerComponent('GodotPlatformApp', () => PlatformApp);
AppRegistry.registerComponent('GodotAssetsApp', () => AssetsApp);
