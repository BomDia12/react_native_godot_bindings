'use strict';

import {useEffect, useState} from 'react';
import NativeGodotScene from '../specs/NativeGodotScene';

export const getBinding = rootTag => NativeGodotScene.getBinding(rootTag);
export const read = (rootTag, handle) => NativeGodotScene.read(rootTag, handle);
export const call = (rootTag, handle, command, args = []) => NativeGodotScene.call(rootTag, handle, command, args);
export const callAsync = (rootTag, handle, command, args = []) => NativeGodotScene.callAsync(rootTag, handle, command, args);
export const onChanged = (rootTag, callback) =>
  NativeGodotScene.onChanged(change => {
    if (change.rootTag === rootTag) {
      callback(change);
    }
  });

export function useGodotScene(rootTag) {
  const [state, setState] = useState(null);
  useEffect(() => {
    let sequence = -1;
    // A root can stop or its binding can change between the event and the read;
    // either way there is nothing current to show.
    const update = () => {
      try {
        const binding = getBinding(rootTag);
        if (!binding.ready) {
          setState(null);
          return;
        }
        const snapshot = read(rootTag, binding.binding);
        sequence = snapshot.sequence;
        setState({rootTag, handle: binding.binding, value: snapshot.payload, sequence});
      } catch {
        setState(null);
      }
    };
    const subscription = onChanged(rootTag, change => {
      if (change.sequence > sequence) {
        update();
      }
    });
    update();
    return () => subscription.remove();
  }, [rootTag]);
  return state;
}
