'use strict';

import {unsupported} from '../../src/unsupported';

export default {
  exitApp: () => unsupported('BackHandler.exitApp'),
  addEventListener: () => unsupported('BackHandler.addEventListener'),
};
