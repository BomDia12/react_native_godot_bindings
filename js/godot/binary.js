'use strict';
import {fromByteArray} from 'base64-js';

export const CHUNK_BYTES = 1024 * 1024;
export const binaryService = () => global.__godotNativeModules.get('GodotBinary');
export const descriptor = data => ({blobId: data.blobId, offset: data.offset, size: data.size, type: data.type ?? ''});
export function utf8Encode(text) {
  const encoded = unescape(encodeURIComponent(text));
  return Uint8Array.from(encoded, value => value.charCodeAt(0));
}
export function utf8Decode(bytes) {
  const output = [];
  let characters = [];
  const append = value => {characters.push(value); if (characters.length >= 8192) {output.push(characters.join('')); characters = [];}};
  for (let i = 0; i < bytes.length;) {
    const first = bytes[i++];
    if (first < 128) { append(String.fromCharCode(first)); continue; }
    const count = first >= 0xc2 && first <= 0xdf ? 1 : first >= 0xe0 && first <= 0xef ? 2 : first >= 0xf0 && first <= 0xf4 ? 3 : 0;
    if (!count) { append('\ufffd'); continue; }
    let code = first & (0x7f >> count);
    let consumed = 0;
    while (consumed < count && i < bytes.length) {
      const next = bytes[i];
      if ((next & 0xc0) !== 0x80 || (consumed === 0 && ((first === 0xe0 && next < 0xa0) || (first === 0xed && next > 0x9f) || (first === 0xf0 && next < 0x90) || (first === 0xf4 && next > 0x8f)))) { break; }
      code = (code << 6) | (next & 63);
      i++; consumed++;
    }
    append(consumed === count ? String.fromCodePoint(code) : '\ufffd');
  }
  output.push(characters.join(''));
  return output.join('');
}
export function readBytes(data) {
  const clean = descriptor(data);
  const bytes = new Uint8Array(clean.size);
  for (let offset = 0; offset < bytes.length; offset += CHUNK_BYTES) {
    bytes.set(binaryService().readChunk(clean, offset, Math.min(CHUNK_BYTES, bytes.length - offset)), offset);
  }
  return bytes;
}
export function readBase64(data) {
  return fromByteArray(readBytes(data));
}
export function createFromParts(parts, id) {
  const service = binaryService();
  const sizes = parts.map(part => part.type === 'blob' ? part.data.size : utf8Encode(part.data).length);
  const total = sizes.reduce((sum, size) => sum + size, 0);
  service.begin(id, total);
  let offset = 0;
  try {
    for (const part of parts) {
      if (part.type === 'blob') {
        for (let source = 0; source < part.data.size; source += CHUNK_BYTES) {
          const chunk = service.readChunk(descriptor(part.data), source, Math.min(CHUNK_BYTES, part.data.size - source));
          service.append(id, offset, chunk); offset += chunk.length;
        }
      } else if (part.type === 'string') {
        const bytes = utf8Encode(part.data);
        for (let source = 0; source < bytes.length; source += CHUNK_BYTES) {
          const chunk = bytes.subarray(source, source + CHUNK_BYTES);
          service.append(id, offset, chunk); offset += chunk.length;
        }
      } else { throw new TypeError('Unsupported Blob part.'); }
    }
    service.finish(id);
  } catch (error) { service.release(id); throw error; }
}

export function readBytesAsync(data) {
  const clean = descriptor(data);
  const service = binaryService();
  let timer;
  let released = false;
  let pinned = false;
  let rejectRead;
  const release = () => {if (!released) {released = true; if (pinned) {service.unpin(clean.blobId);}}};
  const promise = new Promise((resolve, reject) => {
    rejectRead = reject;
    try {
      service.pin(clean); pinned = true;
      const bytes = new Uint8Array(clean.size);
      let offset = 0;
      const step = () => {
        try {
          if (released) {return;}
          const length = Math.min(CHUNK_BYTES, bytes.length - offset);
          bytes.set(service.readChunk(clean, offset, length), offset); offset += length;
          if (offset === bytes.length) {release(); resolve(bytes);}
          else {timer = setTimeout(step, 0);}
        } catch (error) {release(); reject(error);}
      };
      timer = setTimeout(step, 0);
    } catch (error) {release(); reject(error);}
  });
  promise.cancel = () => {if (!released) {clearTimeout(timer); release(); rejectRead(Object.assign(new Error('Blob read canceled'), {code: 'E_CANCELLED'}));}};
  return promise;
}
