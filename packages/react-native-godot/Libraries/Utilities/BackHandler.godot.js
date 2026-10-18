'use strict';

import {unsupported} from '../../src/unsupported';

// Godot reports no hardware back presses, so, as on iOS, listeners never fire.
export default {
  exitApp: () => unsupported('BackHandler.exitApp'),
  addEventListener: () => ({remove() {}}),
};
