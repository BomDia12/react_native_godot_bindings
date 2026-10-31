'use strict';

import {useEffect, useState} from 'react';
import NativeGodotScene from '../specs/NativeGodotScene';

// Godot connects to a module's signals once JavaScript first asks for the
// module, so subscribing asks for it.
export function onModuleChanged(name, callback) {
  const subscription = NativeGodotScene.onChanged(change => {
    if (change.module === name) {
      callback(change);
    }
  });
  NativeGodotScene.getModule(name);
  return subscription;
}

// The GDScript object registered with ReactNativeHost.register_module(name).
// A module registered again later needs a fresh getGodotModule().
export function getGodotModule(name) {
  const {ready, binding} = NativeGodotScene.getModule(name);
  return {
    ready,
    read: () => NativeGodotScene.readModule(name, binding),
    call: (command, ...args) => NativeGodotScene.callModule(name, binding, command, args),
    callAsync: (command, ...args) => NativeGodotScene.callModuleAsync(name, binding, command, args),
    onChanged: callback => onModuleChanged(name, callback),
  };
}

// The module and its latest snapshot, or null while it isn't registered.
export function useGodotModule(name) {
  const [state, setState] = useState(null);
  useEffect(() => {
    let sequence = -1;
    const update = () => {
      try {
        const module = getGodotModule(name);
        if (!module.ready) {
          setState(null);
          return;
        }
        const snapshot = module.read();
        sequence = snapshot.sequence;
        setState({module, value: snapshot.payload, sequence});
      } catch {
        setState(null);
      }
    };
    const subscription = onModuleChanged(name, change => {
      if (change.sequence > sequence) {
        update();
      }
    });
    update();
    return () => subscription.remove();
  }, [name]);
  return state;
}
