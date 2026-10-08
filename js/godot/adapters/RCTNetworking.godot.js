'use strict';

import {toByteArray} from 'base64-js';
import EventEmitter from 'react-native/Libraries/vendor/emitter/EventEmitter';
import convertRequestBody from 'react-native/Libraries/Network/convertRequestBody';
import {binaryService, descriptor, createFromParts, utf8Decode, readBytes, readBase64, utf8Encode} from '../binary';

const emitter = new EventEmitter();
const requests = new Map();
let nextId = 1;
const native = () => global.__godotNativeModules.get('GodotHTTP');
function borrowedBlob(value, pinned) {
  const clean = descriptor(value);
  if (!pinned.has(clean.blobId)) {
    binaryService().pin(clean);
    pinned.add(clean.blobId);
  }
  return clean;
}
function bytesBody(value, owned, pinned) {
  if (value == null) { return null; }
  if (value.blob) { return borrowedBlob(value.blob, pinned); }
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
    const pinned = new Set();
    let cleaned = false;
    const cleanup = () => {
      if (cleaned) {return;}
      cleaned = true;
      for (const blobId of pinned) {binaryService().unpin(blobId);}
      for (const blobId of owned) {binaryService().release(blobId);}
    };
    let terminal = false;
    const complete = (error, timedOut = false) => {if (!terminal) {terminal = true; emitter.emit('didCompleteNetworkResponse', [id, error, timedOut]);}};
    let operation;
    let notified = false;
    const notify = () => {if (!notified) {notified = true; callback(id);}};
    try {
      const converted = convertRequestBody(body);
      const request = {method, url, headers, timeout, credentials: withCredentials, body: converted?.formData ? null : bytesBody(converted, owned, pinned)};
      if (converted?.formData) {
        request.formData = converted.formData.map(part => {
          const clean = {headers: part.headers};
          if (part.string != null) { clean.blob = bytesBody({string: part.string}, owned, pinned); }
          else if (part.blob || part.data?.blobId || part._data?.blobId) { clean.blob = borrowedBlob(part.blob ?? part.data ?? part._data, pinned); }
          else if (part.uri) { clean.uri = part.uri; }
          return clean;
        });
      }
      operation = native().__godotStartAsync('send', [request]);
      requests.set(id, operation.requestId);
      operation.promise.then(response => {
        let transferred = false;
        try {
          const responseHeaders = Object.create(null);
          const headerNames = new Map();
          for (const [name, value] of response.headers) {
            const normalized = name.toLowerCase();
            const key = headerNames.get(normalized) ?? name;
            headerNames.set(normalized, key);
            responseHeaders[key] = responseHeaders[key] == null ? value : responseHeaders[key] + ', ' + value;
          }
          emitter.emit('didReceiveNetworkResponse', [id, response.status, responseHeaders, response.url]);
          const value = responseType === 'blob' ? {...response.body, type: response.headers.find(([name]) => name.toLowerCase() === 'content-type')?.[1] ?? ''} : responseType === 'base64' ? readBase64(response.body) : utf8Decode(readBytes(response.body));
          emitter.emit('didReceiveNetworkData', [id, value]);
          transferred = responseType === 'blob';
          complete(null);
        } finally {
          if (!transferred) { binaryService().release(response.body.blobId); }
        }
      }, error => complete(error.message, error.code === 'E_TIMEOUT'))
        .catch(error => complete(error.message))
        .finally(() => { requests.delete(id); cleanup(); });
      notify();
    } catch (error) {
      if (operation) {global.__godotNativeModules.cancel(operation.requestId);}
      requests.delete(id);
      cleanup();
      queueMicrotask(() => complete(error.message));
      notify();
    }
  },
  abortRequest(id) { const token = requests.get(id); if (token) { global.__godotNativeModules.cancel(token); } },
  clearCookies(callback) { callback(native().clearCookies()); },
};
