'use strict';

const dynamicColor = names => ({$godot: 'DynamicColor', names});

export const PlatformColor = (...names) => dynamicColor(names);
export const normalizeColorObject = color =>
  color?.$godot === 'DynamicColor' ? color : null;
export const processColorObject = color => {
  if (color?.$godot === 'DynamicColor') {
    const error = new Error(
      'PlatformColor is pending on the Godot platform.',
    );
    error.code = 'E_UNSUPPORTED';
    error.operation = 'PlatformColor';
    throw error;
  }
  return null;
};
