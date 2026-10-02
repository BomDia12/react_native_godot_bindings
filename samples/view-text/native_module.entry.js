import React, {useContext, useEffect, useState} from 'react';
import {AppRegistry, Text, View} from 'react-native';
import {RootTagContext} from 'react-native/Libraries/ReactNative/RootTag';
import * as TurboModuleRegistry from 'react-native/Libraries/TurboModule/TurboModuleRegistry';

import {callAsync, closeSession, openSession} from '../../js/godot/modules';
import {color, int64, vector2} from '../../js/godot/values';

function NativeModuleApp() {
  const rootTag = useContext(RootTagContext);
  const [value, setValue] = useState(-1);
  useEffect(() => {
    let session = null;
    let subscription = null;
    let cancelled = false;
    const timer = setTimeout(() => {
      if (cancelled) {
        return;
      }
      const scene = TurboModuleRegistry.getEnforcing('ExampleScene');
      session = openSession(rootTag);
      const target = scene.getTarget(session);
      const echoed = scene.echo({
        color: color(0.2, 0.4, 0.6, 1),
        position: vector2(3, 4),
        integer: int64('9223372036854775807'),
        bytes: new Uint8Array([1, 2, 3, 4]).subarray(1, 3),
      });
      const nativeState = {
        rootTag,
        session,
        target,
        constants: scene.getConstants(),
        schema: scene.__godotSchema,
        echo: echoed,
        before: scene.read(session, target),
        event: null,
        result: null,
        removedResult: null,
        cancelledCode: null,
        error: null,
      };
      global.__godotNativeModuleStates ??= {};
      global.__godotNativeModuleStates[rootTag] = nativeState;
      subscription = scene.onChanged(session, event => {
        nativeState.event = event;
        setValue(event.value);
      });
      callAsync(scene, 'incrementLater', [session, target, 2], {}).then(
        result => {
          nativeState.result = result;
          subscription.remove();
          callAsync(scene, 'incrementLater', [session, target, 1], {}).then(
            removedResult => {
              nativeState.removedResult = removedResult;
            },
            error => {
              nativeState.error = {
                code: error.code,
                message: error.message,
              };
            },
          );
        },
        error => {
          nativeState.error = {
            code: error.code,
            message: error.message,
          };
        },
      );
      const controller = new AbortController();
      callAsync(scene, 'incrementLater', [session, target, 5], {
        signal: controller.signal,
      }).catch(error => {
        nativeState.cancelledCode = error.code;
      });
      controller.abort();
    }, 0);
    return () => {
      cancelled = true;
      clearTimeout(timer);
      subscription?.remove();
      if (session != null) {
        closeSession(session);
      }
    };
  }, [rootTag]);
  return (
    <View>
      <Text>{`Native value ${value}`}</Text>
    </View>
  );
}

AppRegistry.registerComponent('GodotNativeModuleApp', () => NativeModuleApp);
