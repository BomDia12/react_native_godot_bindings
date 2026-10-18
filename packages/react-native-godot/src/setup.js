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
    require('react-native/Libraries/Utilities/HMRClient').default.setup('godot', bundleEntry, host, port, true, scheme);
  }
}
