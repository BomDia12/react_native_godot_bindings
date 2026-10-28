import React, {useEffect, useRef, useState} from 'react';
import {AppRegistry, BackHandler, findNodeHandle, Pressable, StyleSheet, Text, View} from 'react-native';
import GodotScene from 'react-native-godot/GodotScene';
import {useGodotRootState} from 'react-native-godot/root';

global.__godotEmbedding = {};
global.__godotRootStates = {};
global.__godotTimers = {fired: 0};
global.__godotScenes = {loads: [], errors: []};
global.__godotGamepad = {presses: [], backPresses: 0};

global.__godotStartTimer = delay => {
  global.__godotTimers.fired = 0;
  setTimeout(() => {
    global.__godotTimers.fired += 1;
  }, delay);
};

// Each root names itself through its initial properties.
function EmbeddingApp({label}) {
  const textRef = useRef(null);
  return (
    <View style={styles.content}>
      <Text
        ref={textRef}
        onLayout={event => {
          const {width} = event.nativeEvent.layout;
          textRef.current?.measureInWindow((x, y) => {
            global.__godotEmbedding[label] = {textWidth: width, windowX: x, windowY: y};
          });
        }}>
        Embedded text
      </Text>
    </View>
  );
}

function RootStateApp({label}) {
  const state = useGodotRootState();
  global.__godotRootStates[label] = state;
  return <View style={styles.fill} />;
}

let setSceneSource = null;
global.__godotSetSceneSource = source => setSceneSource(source);

function GodotSceneApp() {
  const [source, setSource] = useState('res://smoke/tests/godot_scene/Panel.tscn');
  setSceneSource = setSource;
  return (
    <View style={styles.row}>
      <GodotScene
        nativeID="panel"
        source={source}
        style={styles.scene}
        onLoad={() => global.__godotScenes.loads.push(source)}
        onError={event => global.__godotScenes.errors.push(event.nativeEvent.message)}
      />
      <GodotScene
        nativeID="sprite"
        source="res://smoke/tests/godot_scene/Sprite.tscn"
        style={styles.scene}
        onLoad={() => global.__godotScenes.loads.push('sprite')}
      />
    </View>
  );
}

function GodotSceneNestedApp() {
  return (
    <View style={styles.fill}>
      <Text>Nested</Text>
    </View>
  );
}

const backListener = () => {
  global.__godotGamepad.backPresses += 1;
  return true;
};
let backSubscription = null;
global.__godotListenForBack = listening => {
  backSubscription?.remove();
  backSubscription = listening ? BackHandler.addEventListener('hardwareBackPress', backListener) : null;
};

// The first button skips the second when focus moves down.
function GamepadApp() {
  const lastRef = useRef(null);
  const [lastTag, setLastTag] = useState(null);
  useEffect(() => setLastTag(findNodeHandle(lastRef.current)), []);
  const press = name => () => global.__godotGamepad.presses.push(name);
  return (
    <View style={styles.fill}>
      <Pressable nativeID="first" focusable hasTVPreferredFocus nextFocusDown={lastTag ?? undefined} onPress={press('first')} style={styles.button} />
      <Pressable nativeID="middle" focusable onPress={press('middle')} style={styles.button} />
      <Pressable nativeID="last" ref={lastRef} focusable onPress={press('last')} style={styles.button} />
    </View>
  );
}

const styles = StyleSheet.create({
  content: {
    height: 48,
    alignItems: 'flex-start',
  },
  fill: {
    flex: 1,
  },
  row: {
    flex: 1,
    flexDirection: 'row',
  },
  scene: {
    width: 100,
    height: 80,
  },
  button: {
    width: 60,
    height: 24,
    marginBottom: 8,
    backgroundColor: '#335577',
  },
});

AppRegistry.registerComponent('GodotEmbeddingApp', () => EmbeddingApp);
AppRegistry.registerComponent('GodotRootStateApp', () => RootStateApp);
AppRegistry.registerComponent('GodotSceneApp', () => GodotSceneApp);
AppRegistry.registerComponent('GodotSceneNestedApp', () => GodotSceneNestedApp);
AppRegistry.registerComponent('GodotGamepadApp', () => GamepadApp);
