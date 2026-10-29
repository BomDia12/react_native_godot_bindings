'use strict';

import React, {useEffect, useMemo, useState} from 'react';
import {callAsync, getBinding, onChanged, read} from './scene';

// Contracts name things in snake_case, as Godot does; components receive
// camelCase props, as React does.
const camelCase = name => name.replace(/_([a-z0-9])/g, (_match, letter) => letter.toUpperCase());
const snakeCase = name => name.replace(/[A-Z]/g, letter => '_' + letter.toLowerCase());

function renameKeys(value, rename) {
  if (Array.isArray(value)) {
    return value.map(element => renameKeys(element, rename));
  }
  if (value === null || typeof value !== 'object' || '$godot' in value) {
    return value;
  }
  return Object.fromEntries(Object.entries(value).map(([key, field]) => [rename(key), renameKeys(field, rename)]));
}

// The root's contract values, starting from the props the surface started
// with. Reading once when the binding connects catches writes made before;
// after that every change carries the values.
function useContract(rootTag, initial) {
  const [contract, setContract] = useState({values: initial, handle: null});
  useEffect(() => {
    let sequence = -1;
    const connect = () => {
      const binding = getBinding(rootTag);
      if (!binding.ready) {
        setContract(current => ({...current, handle: null}));
        return;
      }
      const snapshot = read(rootTag, binding.binding);
      sequence = snapshot.sequence;
      setContract({values: snapshot.payload, handle: binding.binding});
    };
    const subscription = onChanged(rootTag, change => {
      if (change.sequence <= sequence) {
        return;
      }
      sequence = change.sequence;
      if (change.event === 'changed') {
        setContract(current => ({...current, values: change.payload.values}));
      } else if (change.event === 'ready') {
        connect();
      }
    });
    connect();
    return () => subscription.remove();
  }, [rootTag]);
  return contract;
}

// Wraps a component whose props a Godot contract declares. events maps each
// contract event to the prop the component calls, such as {mark: 'onMark'}.
export function withGodotContract(Component, events = {}) {
  return function GodotContractRoot({rootTag, ...initial}) {
    const {values, handle} = useContract(rootTag, initial);
    const handlers = useMemo(
      () =>
        Object.fromEntries(
          Object.entries(events).map(([event, prop]) => [
            prop,
            (...args) => {
              if (handle == null) {
                console.warn(`The ${event} event was dropped because the root's contract is not connected.`);
                return;
              }
              callAsync(rootTag, handle, event, renameKeys(args, snakeCase)).catch(error => console.error(error));
            },
          ]),
        ),
      [rootTag, handle],
    );
    return <Component {...renameKeys(values, camelCase)} {...handlers} />;
  };
}
