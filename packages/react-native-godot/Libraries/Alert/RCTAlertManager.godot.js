'use strict';

import NativeAlertManager from 'react-native/Libraries/Alert/NativeAlertManager';

export function alertWithArgs(args, callback) {
  NativeAlertManager?.alertWithArgs(args, callback);
}
