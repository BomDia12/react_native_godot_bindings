'use strict';

import {useContext, useEffect, useState} from 'react';
import {RootTagContext} from 'react-native';
import NativeGodotHost from '../specs/NativeGodotHost';

export const getRootState = rootTag => NativeGodotHost.getRootState(rootTag);

export const onRootStateChanged = (rootTag, callback) =>
  NativeGodotHost.onRootStateChanged(({rootTag: changedRoot, ...state}) => {
    if (changedRoot === rootTag) {
      callback(state);
    }
  });

// The visibility, processing and window focus of the root the calling
// component renders in.
export function useGodotRootState() {
  const rootTag = useContext(RootTagContext);
  const [state, setState] = useState(() => getRootState(rootTag));
  useEffect(() => {
    const subscription = onRootStateChanged(rootTag, setState);
    // The state can change between the first render and the subscription.
    setState(getRootState(rootTag));
    return () => subscription.remove();
  }, [rootTag]);
  return state;
}

// The data of the drag going on in the root the calling component renders in,
// or null, so views can show where it can drop.
export function useGodotDrag() {
  const rootTag = useContext(RootTagContext);
  const [data, setData] = useState(null);
  useEffect(() => {
    const subscription = NativeGodotHost.onDragChanged(change => {
      if (change.rootTag === rootTag) {
        setData(change.data ?? null);
      }
    });
    return () => subscription.remove();
  }, [rootTag]);
  return data;
}
