'use strict';

import {useCallback, useContext, useEffect, useRef} from 'react';
import {RootTagContext} from 'react-native';
import NativeGodotHost from '../specs/NativeGodotHost';

// Without a root tag, messages go to and come from ReactNativeHost instead of
// one root.
export function postGodotMessage(name, payload = null, rootTag = null) {
  NativeGodotHost.postMessage(rootTag, name, payload);
}

export function onGodotMessage(name, handler, rootTag = null) {
  return NativeGodotHost.onMessage(message => {
    if (message?.name === name && message.rootTag === rootTag) {
      handler(message.payload);
    }
  });
}

// Receives the messages Godot posts to the root the calling component renders in.
export function useGodotMessage(name, handler) {
  const rootTag = useContext(RootTagContext);
  const latestHandler = useRef(handler);
  latestHandler.current = handler;
  useEffect(() => {
    const subscription = onGodotMessage(name, payload => latestHandler.current(payload), rootTag);
    return () => subscription.remove();
  }, [name, rootTag]);
}

export function usePostGodotMessage() {
  const rootTag = useContext(RootTagContext);
  return useCallback((name, payload) => postGodotMessage(name, payload, rootTag), [rootTag]);
}
