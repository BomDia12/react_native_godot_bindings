import React, {useContext, useEffect, useState} from 'react';
import {AppRegistry, Text, View} from 'react-native';
import {RootTagContext} from 'react-native/Libraries/ReactNative/RootTag';
import {NativeExampleScene} from 'react-native-godot-example';
import {call, getBinding} from 'react-native-godot/scene';
import {color, int64, vector2} from 'react-native-godot/values';

function NativeModuleApp() {
  const rootTag = useContext(RootTagContext);
  const [value, setValue] = useState(-1);
  useEffect(() => {
    const state = {
      rootTag,
      constants: NativeExampleScene.getConstants(),
      before: NativeExampleScene.read(rootTag),
      echo: null,
      event: null,
      result: null,
      removedResult: null,
      error: null,
    };
    global.__godotNativeModuleStates ??= {};
    global.__godotNativeModuleStates[rootTag] = state;
    const binding = getBinding(rootTag);
    state.echo = call(rootTag, binding.binding, 'echo', [
      {
        color: color(0.2, 0.4, 0.6, 1),
        position: vector2(3, 4),
        integer: int64('9223372036854775807'),
        bytes: new Uint8Array([1, 2, 3, 4]).subarray(1, 3),
      },
    ]);
    // onChanged reports every root's counter, so the roots take turns: each
    // subscribes only once the previous root has unsubscribed.
    let subscription = null;
    const fail = error => {
      state.error = {code: error.code, message: error.message};
    };
    global.__godotNativeModuleTurn = (global.__godotNativeModuleTurn ?? Promise.resolve())
      .then(() => {
        subscription = NativeExampleScene.onChanged(event => {
          state.event = event;
          setValue(event.value);
        });
        return NativeExampleScene.incrementLater(rootTag, 2);
      })
      .then(result => {
        state.result = result;
        subscription.remove();
        return NativeExampleScene.incrementLater(rootTag, 1);
      })
      .then(result => {
        state.removedResult = result;
      }, fail);
    return () => subscription?.remove();
  }, [rootTag]);
  return (
    <View>
      <Text>{`Native value ${value}`}</Text>
    </View>
  );
}

AppRegistry.registerComponent('GodotNativeModuleApp', () => NativeModuleApp);
