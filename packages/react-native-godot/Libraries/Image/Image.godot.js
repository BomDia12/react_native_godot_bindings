'use strict';
import React from 'react';
import ImageViewNativeComponent from 'react-native/Libraries/Image/ImageViewNativeComponent';
import NativeImageLoaderAndroid from 'react-native/Libraries/Image/NativeImageLoaderAndroid';
import resolveAssetSource from 'react-native/Libraries/Image/resolveAssetSource';
import StyleSheet from 'react-native/Libraries/StyleSheet/StyleSheet';

let nextRequestId = 1;

const Image = React.forwardRef(function Image({source, src, style, resizeMode, tintColor, ...props}, ref) {
  const resolved = src != null ? {uri: src} : resolveAssetSource(Array.isArray(source) ? source[0] : source);
  const flattened = StyleSheet.flatten(style) ?? {};
  const size = resolved?.width != null && resolved?.height != null ? {width: resolved.width, height: resolved.height} : null;
  return (
    <ImageViewNativeComponent
      {...props}
      ref={ref}
      source={resolved}
      resizeMode={resizeMode ?? flattened.resizeMode ?? 'cover'}
      tintColor={tintColor ?? flattened.tintColor}
      style={[size, style]}
    />
  );
});

Image.resolveAssetSource = resolveAssetSource;

Image.getSize = (uri, success, failure) => {
  const promise = NativeImageLoaderAndroid.getSize(uri);
  if (typeof success === 'function') {
    promise.then(size => success(size.width, size.height), failure);
  }
  return promise;
};

Image.getSizeWithHeaders = (uri, headers, success, failure) => {
  const promise = NativeImageLoaderAndroid.getSizeWithHeaders(uri, headers);
  if (typeof success === 'function') {
    promise.then(size => success(size.width, size.height), failure);
  }
  return promise;
};

Image.prefetch = (url, callback) => {
  const requestId = nextRequestId++;
  callback?.(requestId);
  return NativeImageLoaderAndroid.prefetchImage(url, requestId);
};

Image.prefetchWithMetadata = url => Image.prefetch(url);

Image.abortPrefetch = requestId => NativeImageLoaderAndroid.abortRequest(requestId);

Image.queryCache = urls => NativeImageLoaderAndroid.queryCache(urls);

export default Image;
