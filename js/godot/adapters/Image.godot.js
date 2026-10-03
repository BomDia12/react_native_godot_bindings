'use strict';
import React from 'react';
import * as NativeComponentRegistry from 'react-native/Libraries/NativeComponent/NativeComponentRegistry';
import * as TurboModuleRegistry from 'react-native/Libraries/TurboModule/TurboModuleRegistry';
import StyleSheet from 'react-native/Libraries/StyleSheet/StyleSheet';
import {colorAttribute} from 'react-native/Libraries/Components/View/ReactNativeStyleAttributes';
import resolveAssetSource from './resolveAssetSource.godot';

const loader = () => TurboModuleRegistry.getEnforcing('GodotImageLoader');
const events = {};
for (const name of ['LoadStart', 'Progress', 'Load', 'Error', 'LoadEnd']) {
  events['top' + name] = {registrationName: 'on' + name};
}
const NativeImage = NativeComponentRegistry.get('RCTImageView', () => ({
  uiViewClassName: 'RCTImageView', directEventTypes: events,
  validAttributes: {
    source: true, resizeMode: true, tintColor: colorAttribute,
    ...Object.fromEntries(Object.values(events).map(event => [event.registrationName, true])),
  },
}));
const sourceRecord = source => typeof source === 'string' ? {uri: source} : source;
const Image = React.forwardRef(function Image({source, src, style, resizeMode, tintColor, ...props}, ref) {
  const resolved = src != null ? {uri: src} : resolveAssetSource(Array.isArray(source) ? source[0] : source);
  const flattened = StyleSheet.flatten(style) ?? {};
  return <NativeImage {...props} ref={ref} source={resolved}
    resizeMode={resizeMode ?? flattened.resizeMode ?? 'cover'}
    tintColor={tintColor ?? flattened.tintColor}
    style={[resolved?.width != null && resolved?.height != null ? {width: resolved.width, height: resolved.height} : null, style]} />;
});
Image.resolveAssetSource = resolveAssetSource;
Image.getSize = (uri, success, failure) => {
  const promise = loader().getSize(sourceRecord(uri));
  if (typeof success === 'function') { promise.then(size => success(size.width, size.height), failure); }
  return promise;
};
Image.getSizeWithHeaders = (uri, headers, success, failure) => Image.getSize({uri, headers}, success, failure);
let nextPrefetch = 1;
const prefetches = new Map();
Image.prefetch = (uri, callback) => {
  const id = nextPrefetch++;
  const module = loader();
  const operation = module.__godotStartAsync('prefetch', [sourceRecord(uri)]);
  prefetches.set(id, operation.requestId);
  callback?.(id);
  return operation.promise.finally(() => prefetches.delete(id));
};
Image.prefetchWithMetadata = uri => Image.prefetch(uri);
Image.abortPrefetch = id => {
  const token = prefetches.get(id);
  if (token != null) { global.__godotNativeModules.cancel(token); prefetches.delete(id); }
};
Image.queryCache = uris => Promise.resolve(loader().queryCache(uris.map(sourceRecord)));
export default Image;
