import './native-facades-install';
'use strict';

if (typeof global.__godotFinalizeBootstrap === 'function') {
  global.__godotFinalizeBootstrap(Boolean(global.__DEV__));
}
