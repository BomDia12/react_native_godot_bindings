'use strict';

import normalizeColor from '@react-native/normalize-colors';
const {processGodotColor} = require('../color.cjs');

export default function processColor(color) {
  return processGodotColor(color, normalizeColor);
}
