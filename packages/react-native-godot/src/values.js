'use strict';

export const int64 = value => ({$godot: 'int64', value: String(value)});
export const color = (r, g, b, a = 1) => ({$godot: 'Color', r, g, b, a});
export const vector2 = (x, y) => ({$godot: 'Vector2', x, y});
export const vector3 = (x, y, z) => ({$godot: 'Vector3', x, y, z});
export const rect2 = (x, y, width, height) => ({
  $godot: 'Rect2',
  x,
  y,
  width,
  height,
});
