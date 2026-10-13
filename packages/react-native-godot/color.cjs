'use strict';

function processGodotColor(color, normalizeColor) {
  if (color === undefined || color === null) {
    return color;
  }
  if (typeof color === 'object' && color.$godot === 'Color') {
    const channels = [color.r, color.g, color.b, color.a];
    if (
      channels.every(
        channel =>
          typeof channel === 'number' &&
          Number.isFinite(channel) &&
          channel >= 0 &&
          channel <= 1,
      )
    ) {
      return color;
    }
    throw new TypeError('Godot Color channels must be finite and in 0-1.');
  }
  const normalized = normalizeColor(color);
  if (normalized === null || normalized === undefined) {
    return undefined;
  }
  if (typeof normalized === 'object') {
    const error = new Error(
      'Theme and dynamic colors are pending on the Godot platform.',
    );
    error.code = 'E_UNSUPPORTED';
    error.operation = 'processColor';
    throw error;
  }
  if (typeof normalized !== 'number') {
    return null;
  }
  const value = normalized >>> 0;
  return {
    $godot: 'Color',
    r: ((value >>> 24) & 0xff) / 255,
    g: ((value >>> 16) & 0xff) / 255,
    b: ((value >>> 8) & 0xff) / 255,
    a: (value & 0xff) / 255,
  };
}

module.exports = {processGodotColor};
