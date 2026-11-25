'use strict';
import React from 'react';
import ImageViewNativeComponent from 'react-native/Libraries/Image/ImageViewNativeComponent';
import NativeImageLoaderAndroid from 'react-native/Libraries/Image/NativeImageLoaderAndroid';
import {getImageSourcesFromImageProps} from 'react-native/Libraries/Image/ImageSourceUtils';
import resolveAssetSource from 'react-native/Libraries/Image/resolveAssetSource';
import StyleSheet from 'react-native/Libraries/StyleSheet/StyleSheet';

let nextRequestId = 1;

// As on iOS, every source reaches the native component, which loads the one
// that fits the laid-out size.
const Image = React.forwardRef(function Image(props, ref) {
  const {source: _source, src: _src, srcSet: _srcSet, style, resizeMode, tintColor, ...rest} = props;
  const source = getImageSourcesFromImageProps(props) ?? {uri: undefined};
  const sources = Array.isArray(source) ? source : [source];
  const size = Array.isArray(source) ? null : {width: source.width ?? props.width, height: source.height ?? props.height};
  const flattened = StyleSheet.flatten(style) ?? {};
  return (
    <ImageViewNativeComponent
      {...rest}
      ref={ref}
      source={sources}
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
