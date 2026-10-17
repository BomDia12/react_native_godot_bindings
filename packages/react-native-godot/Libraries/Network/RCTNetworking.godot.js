'use strict';

import RCTDeviceEventEmitter from 'react-native/Libraries/EventEmitter/RCTDeviceEventEmitter';
import NativeNetworkingIOS from 'react-native/Libraries/Network/NativeNetworkingIOS';
import convertRequestBody from 'react-native/Libraries/Network/convertRequestBody';

const RCTNetworking = {
  addListener(eventType, listener, context) {
    return RCTDeviceEventEmitter.addListener(eventType, listener, context);
  },

  sendRequest(method, trackingName, url, headers, data, responseType, incrementalUpdates, timeout, callback, withCredentials) {
    NativeNetworkingIOS.sendRequest(
      {
        method,
        url,
        data: {...convertRequestBody(data), trackingName},
        headers,
        responseType,
        incrementalUpdates,
        timeout,
        withCredentials,
      },
      callback,
    );
  },

  abortRequest(requestId) {
    NativeNetworkingIOS.abortRequest(requestId);
  },

  clearCookies(callback) {
    NativeNetworkingIOS.clearCookies(callback);
  },
};

export default RCTNetworking;
