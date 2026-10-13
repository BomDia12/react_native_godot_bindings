'use strict';

function pickScale(scales, deviceScale = 1) {
  if (!Array.isArray(scales) || scales.length === 0) {
    throw new TypeError('Asset scales must be a non-empty array.');
  }
  for (const scale of scales) {
    if (scale >= deviceScale) {
      return scale;
    }
  }
  return scales[scales.length - 1];
}

module.exports = {pickScale};
