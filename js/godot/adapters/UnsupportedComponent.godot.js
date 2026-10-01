'use strict';

import React from 'react';

export default function UnsupportedGodotComponent() {
  global.__godotUnsupported('Android-only component');
  return null;
}
