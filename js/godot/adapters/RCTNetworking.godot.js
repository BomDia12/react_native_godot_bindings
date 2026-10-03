'use strict';

import {toByteArray} from 'base64-js';
import EventEmitter from 'react-native/Libraries/vendor/emitter/EventEmitter';
import convertRequestBody from 'react-native/Libraries/Network/convertRequestBody';
import {binaryService, descriptor, createFromParts, utf8Decode, readBytes, readBase64, utf8Encode} from '../binary';

const emitter = new EventEmitter();
const requests = new Map();
let nextId = 1;
const native = () => global.__godotNativeModules.get('GodotHTTP');
function bytesBody(value, owned) {
  if (value == null) { return null; }
  if (value.blob) { return descriptor(value.blob); }
  if (value.string != null) {
    const id = 'upload-' + nextId++;
    createFromParts([{type: 'string', data: value.string}], id);
    owned.push(id);
    return {blobId: id, offset: 0, size: utf8Encode(value.string).length, type: ''};
  }
  if (value.base64 != null) {
    const bytes = toByteArray(value.base64);
    const id = 'upload-' + nextId++;
    const service = binaryService(); service.begin(id, bytes.length); owned.push(id);
    for (let offset = 0; offset < bytes.length; offset += 1024 * 1024) {service.append(id, offset, bytes.subarray(offset, offset + 1024 * 1024));}
    service.finish(id); return {blobId: id, offset: 0, size: bytes.length, type: ''};
  }
  if (value.uri) { return value; }
  return null;
}
export default {
  addListener: (name, callback, context) => emitter.addListener(name, callback, context),
  sendRequest(method, trackingName, url, headers, body, responseType, incrementalUpdates, timeout, callback, withCredentials) {
    const id = nextId++;
    const owned = [];
    let terminal = false;
    const complete = (error, timedOut = false) => {if (!terminal) {terminal = true; emitter.emit('didCompleteNetworkResponse', [id, error, timedOut]);}};
    const converted = convertRequestBody(body);
    let request;
    try {
      request = {method, url, headers, timeout, credentials: withCredentials, body: converted?.formData ? null : bytesBody(converted, owned)};
      if (converted?.formData) {
        request.formData = converted.formData.map(part => {
          const clean = {headers: part.headers};
          if (part.string != null) { clean.blob = bytesBody({string: part.string}, owned); }
          else if (part.blob || part.data?.blobId || part._data?.blobId) { clean.blob = descriptor(part.blob ?? part.data ?? part._data); }
          else if (part.uri) { clean.uri = part.uri; }
          return clean;
        });
      }
      const operation = native().__godotStartAsync('send', [request]);
      requests.set(id, operation.requestId);
      callback(id);
      operation.promise.then(response => {
        const responseHeaders = {};
        for (const [name, value] of response.headers) {
          responseHeaders[name] = responseHeaders[name] == null ? value : responseHeaders[name] + ', ' + value;
        }
        emitter.emit('didReceiveNetworkResponse', [id, response.status, responseHeaders, response.url]);
        let value;
        try {
          value = responseType === 'blob' ? response.body : responseType === 'base64' ? readBase64(response.body) : utf8Decode(readBytes(response.body));
          emitter.emit('didReceiveNetworkData', [id, value]);
          complete(null);
        } finally {
          if (responseType !== 'blob') { binaryService().release(response.body.blobId); }
        }
      }, error => complete(error.message, error.code === 'E_TIMEOUT'))
        .catch(error => complete(error.message))
        .finally(() => { requests.delete(id); for (const blobId of owned) { binaryService().release(blobId); } });
    } catch (error) {
      callback(id);
      for (const blobId of owned) { binaryService().release(blobId); }
      queueMicrotask(() => complete(error.message));
    }
  },
  abortRequest(id) { const token = requests.get(id); if (token) { global.__godotNativeModules.cancel(token); } },
  clearCookies(callback) { callback(native().clearCookies()); },
};
