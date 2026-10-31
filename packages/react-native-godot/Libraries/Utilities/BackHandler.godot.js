'use strict';

import RCTDeviceEventEmitter from 'react-native/Libraries/EventEmitter/RCTDeviceEventEmitter';
import {HardwareBackPressEvent} from 'react-native/Libraries/Utilities/HardwareBackPressEvent';
import NativeGodotHost from '../../specs/NativeGodotHost';
import {unsupported} from '../../src/unsupported';

// Godot's ui_cancel action, a gamepad's B button by default, is the back
// button. Godot can't wait for JavaScript to decide whether a press was
// handled, so roots take ui_cancel from the game only while listeners exist,
// and give it back, as upstream's default handler does, when none handles it.
const subscriptions = [];

RCTDeviceEventEmitter.addListener('hardwareBackPress', () => {
  const event = new HardwareBackPressEvent({});
  for (let i = subscriptions.length - 1; i >= 0; i--) {
    if (subscriptions[i](event)) {
      return;
    }
  }
  NativeGodotHost.invokeDefaultBackPressHandler();
});

function setSubscriptionCount(count) {
  if ((count === 0) !== (subscriptions.length === 0)) {
    NativeGodotHost.setBackHandlerEnabled(count > 0);
  }
}

export default {
  exitApp: () => unsupported('BackHandler.exitApp'),
  addEventListener(eventName, handler) {
    if (!subscriptions.includes(handler)) {
      setSubscriptionCount(subscriptions.length + 1);
      subscriptions.push(handler);
    }
    return {
      remove() {
        const index = subscriptions.indexOf(handler);
        if (index !== -1) {
          setSubscriptionCount(subscriptions.length - 1);
          subscriptions.splice(index, 1);
        }
      },
    };
  },
};
