'use strict';

import {useEffect, useState} from 'react';
import {openSession, closeSession, callAsync as nativeCallAsync} from './modules';

const scene = () => global.__godotNativeModules.get('GodotScene');
export const getBinding = session => scene().getBinding(session);
export const read = (session, handle) => scene().read(session, handle);
export const call = (session, handle, command, args = []) =>
  scene().call(session, handle, command, args);
export const callAsync = (session, handle, command, args = [], options = {}) =>
  nativeCallAsync(scene(), 'callAsync', [session, handle, command, args], options);
export const onChanged = (session, callback) => scene().onChanged(session, callback);

export function useGodotScene(rootTag) {
  const [state, setState] = useState(null);
  useEffect(() => {
    const session = openSession(rootTag);
    let current = null;
    let sequence = -1;
    let disposed = false;
    const update = () => {
      const binding = getBinding(session);
      if (!binding.ready) {
        current = null;
        setState(null);
        return;
      }
      current = binding.binding;
      const snapshot = read(session, current);
      sequence = snapshot.sequence;
      setState({session, handle: current, value: snapshot.payload, sequence});
    };
    const subscription = onChanged(session, event => {
      if (disposed || event.sequence <= sequence) {
        return;
      }
      update();
    });
    update();
    return () => {
      disposed = true;
      subscription.remove();
      closeSession(session);
    };
  }, [rootTag]);
  return state;
}
