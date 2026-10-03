'use strict';

import DeviceEventEmitter from 'react-native/Libraries/EventEmitter/RCTDeviceEventEmitter';
import {toByteArray, fromByteArray} from 'base64-js';
import {utf8Encode, utf8Decode, descriptor, CHUNK_BYTES} from './binary';

let module;
const blobModes = new Set();
function native() {
  if (module == null) {
    module = global.__godotNativeModules.get('GodotWebSocket');
    global.__godotScheduler.withoutOrigin(() => module.onEvent(({name, payload}) => {
      if (name === 'websocketMessage') {
        const bytes = payload.bytes;
        if (payload.text) {
          payload = {id: payload.id, type: 'text', data: utf8Decode(bytes)};
        } else if (blobModes.has(payload.id)) {
          const service = global.__godotNativeModules.get('GodotBinary');
          const id = 'socket-' + payload.id + '-' + sequence++;
          service.begin(id, bytes.length);
          try {
            for (let offset = 0; offset < bytes.length; offset += CHUNK_BYTES) {service.append(id, offset, bytes.subarray(offset, offset + CHUNK_BYTES));}
            service.finish(id);
          } catch (error) {service.release(id); throw error;}
          payload = {id: payload.id, type: 'blob', data: {blobId: id, offset: 0, size: bytes.length, type: ''}};
        } else { payload = {id: payload.id, type: 'binary', data: fromByteArray(bytes)}; }
      }
      if (name === 'websocketClosed' || name === 'websocketFailed') {blobModes.delete(payload.id);}
      DeviceEventEmitter.emit(name, payload);
    }));
  }
  return module;
}
let sequence = 1;
export const websocketFacade = {
  addListener() {}, removeListeners() {},
  connect: (url, protocols, options, id) => native().connect(url, protocols ?? [], options?.headers ?? {}, id),
  send: (text, id) => native().send(id, utf8Encode(text), true),
  sendBinary: (base64, id) => native().send(id, toByteArray(base64), false),
  close: (code, reason, id) => native().close(id, code, reason),
  ping: () => global.__godotUnsupported('WebSocket.ping'),
};
export const socketBlobMode = (id, enabled) => enabled ? blobModes.add(id) : blobModes.delete(id);
export const sendSocketBlob = (data, id) => native().sendBlob(id, descriptor(data));
