'use strict';
import {websocketFacade, socketBlobMode, sendSocketBlob} from './websocket-facade';
import {fromByteArray} from 'base64-js';
import {binaryService, createFromParts, readBytesAsync, utf8Decode} from './binary';

let services;
let emitterSubscription;
const facades = new Map();
function applicationServices() {
  services ??= global.__godotNativeModules.get('GodotServices');
  if (emitterSubscription == null) {
    emitterSubscription = global.__godotScheduler.withoutOrigin(() => services.onEvent(({name, payload}) => {
      require('react-native/Libraries/EventEmitter/RCTDeviceEventEmitter').default.emit(name, payload);
    }));
  }
  return services;
}
export function nativeFacade(name) {
  if (facades.has(name)) {
    return facades.get(name);
  }
  let facade;
  const listeners = {addListener() {}, removeListeners() {}};
  if (name === 'Clipboard') {
    facade = {getConstants: () => ({}), getString: () => Promise.resolve().then(() => applicationServices().clipboardGet()), setString: value => applicationServices().clipboardSet(value)};
  } else if (name === 'KeyboardObserver') {
    applicationServices(); facade = listeners;
  } else if (name === 'LinkingManager') {
    facade = {...listeners, getInitialURL: () => Promise.resolve(null),
      openURL: url => Promise.resolve().then(() => applicationServices().openURL(url)),
      canOpenURL: () => Promise.reject(Object.assign(new Error('Godot has no URL-handler query provider.'), {code: 'E_UNSUPPORTED'})),
      openSettings: () => Promise.reject(Object.assign(new Error('Godot has no application-settings provider.'), {code: 'E_UNSUPPORTED'})),
    };
  } else if (name === 'Vibration') {
    facade = {vibrate: (duration = 400) => applicationServices().vibrate(duration), cancel: () => applicationServices().cancelVibration()};
  } else if (name === 'WebSocketModule') { facade = websocketFacade; } else if (name === 'BlobModule') {
    facade = {getConstants: () => ({BLOB_URI_SCHEME: 'blob'}),
      createFromParts, release: id => binaryService().release(id),
      addNetworkingHandler() {}, addWebSocketHandler: id => socketBlobMode(id, true), removeWebSocketHandler: id => socketBlobMode(id, false),
      sendOverSocket: (data, socketId) => sendSocketBlob(data, socketId),
    };
  } else if (name === 'FileReaderModule') {
    const read = (data, encoding, asURL) => {
      if (encoding && !['utf-8', 'utf8'].includes(encoding.toLowerCase())) {return Promise.reject(new Error('Only UTF-8 Blob text decoding is supported.'));}
      const bytes = readBytesAsync(data);
      const result = bytes.then(value => asURL ? 'data:' + (data.type || 'application/octet-stream') + ';base64,' + fromByteArray(value) : utf8Decode(value));
      result.cancel = bytes.cancel; return result;
    };
    facade = {readAsText: (data, encoding) => read(data, encoding, false), readAsDataURL: data => read(data, null, true)};
  } else if (name === 'DeviceInfo') {
    facade = {getConstants: () => ({Dimensions: applicationServices().getState().Dimensions})};
  } else if (name === 'Appearance') {
    facade = {...listeners,
      getColorScheme: () => applicationServices().getState().colorScheme,
      setColorScheme: value => applicationServices().setColorScheme(value),
    };
  } else if (name === 'AppState') {
    applicationServices();
    facade = {...listeners,
      getConstants: () => ({initialAppState: applicationServices().getState().initialAppState}),
      getCurrentAppState: (success, failure) => {
        try { success({app_state: applicationServices().getState().initialAppState}); }
        catch (error) { failure?.(error); }
      },
    };
  } else if (name === 'I18nManager') {
    facade = {getConstants: () => applicationServices().getState().direction,
      allowRTL: value => applicationServices().setDirection('allow_rtl', value),
      forceRTL: value => applicationServices().setDirection('force_rtl', value),
      swapLeftAndRightInRTL: value => applicationServices().setDirection('swap_rtl', value),
    };
  }
  if (facade != null) {
    facades.set(name, facade);
  }
  return facade;
}
