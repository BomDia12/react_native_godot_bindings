'use strict';

import getDevServer from 'react-native/Libraries/Core/Devtools/getDevServer';
import resolveAssetSource from 'react-native/Libraries/Image/resolveAssetSource';
import {godotAssetSource} from './asset-source';

resolveAssetSource.setCustomSourceTransformer(godotAssetSource);

// Other platforms start Fast Refresh from native code once Metro's bundle has
// loaded. Godot's host has no handle on React Native's callable modules.
if (__DEV__) {
  const {bundleLoadedFromServer, fullBundleUrl} = getDevServer();
  const server = bundleLoadedFromServer && /^(https?):\/\/([^/:]+)(?::(\d+))?\/([^?]+)/.exec(fullBundleUrl);
  if (server) {
    const [, scheme, host, port = '', bundleEntry] = server;
    const HMRClient = require('react-native/Libraries/Utilities/HMRClient').default;
    const Platform = require('react-native/Libraries/Utilities/Platform').default;
    HMRClient.setup('godot', bundleEntry, host, port, Platform.constants.fastRefresh !== false, scheme);
    // The editor turns react_native/dev_server/fast_refresh on and off without a reload.
    require('react-native/Libraries/EventEmitter/RCTDeviceEventEmitter').default.addListener('godotFastRefreshChanged', ({enabled}) =>
      enabled ? HMRClient.enable() : HMRClient.disable(),
    );
  }
}
