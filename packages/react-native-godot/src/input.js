'use strict';

import {useEffect, useRef, useState} from 'react';
import NativeGodotInput from '../specs/NativeGodotInput';

export const GodotInput = {
  getActions: () => NativeGodotInput.getActions(),
  getActionEvents: action => NativeGodotInput.getActionEvents(action),
  setActionEvents: (action, events) => NativeGodotInput.setActionEvents(action, events),
  resetAction: action => NativeGodotInput.resetAction(action),
  captureNextEvent: ({devices = ['keyboard', 'mouse', 'joypad']} = {}) => NativeGodotInput.captureNextEvent(devices),
  cancelCapture: () => NativeGodotInput.cancelCapture(),
  getLastDevice: () => NativeGodotInput.getLastDevice(),
  getJoypads: () => NativeGodotInput.getJoypads(),
  vibrateJoypad: (device, {weak = 0, strong = 0, durationMs = 0} = {}) => NativeGodotInput.vibrateJoypad(device, weak, strong, durationMs),
  stopJoypadVibration: device => NativeGodotInput.stopJoypadVibration(device),
  onAction(action, handler) {
    NativeGodotInput.watchAction(action);
    const subscription = NativeGodotInput.onAction(event => {
      if (event.action === action) {
        handler(event);
      }
    });
    return {
      remove() {
        subscription.remove();
        NativeGodotInput.unwatchAction(action);
      },
    };
  },
  onLastDeviceChanged: handler => NativeGodotInput.onLastDeviceChanged(handler),
  onJoypadsChanged: handler => NativeGodotInput.onJoypadsChanged(handler),
};

export function useInputAction(action, handler) {
  const latestHandler = useRef(handler);
  latestHandler.current = handler;
  useEffect(() => {
    const subscription = GodotInput.onAction(action, event => latestHandler.current(event));
    return () => subscription.remove();
  }, [action]);
}

export function useLastInputDevice() {
  const [device, setDevice] = useState(() => NativeGodotInput.getLastDevice());
  useEffect(() => {
    setDevice(NativeGodotInput.getLastDevice());
    const subscription = NativeGodotInput.onLastDeviceChanged(setDevice);
    return () => subscription.remove();
  }, []);
  return device;
}
