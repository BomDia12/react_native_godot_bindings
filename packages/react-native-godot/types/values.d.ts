import type {GodotColor} from '../godot';

export const int64: (value: bigint | number | string) => {$godot: 'int64'; value: string};
export const color: (r: number, g: number, b: number, a?: number) => GodotColor;
export const vector2: (x: number, y: number) => {$godot: 'Vector2'; x: number; y: number};
export const vector3: (x: number, y: number, z: number) => {$godot: 'Vector3'; x: number; y: number; z: number};
export const rect2: (x: number, y: number, width: number, height: number) => {$godot: 'Rect2'; x: number; y: number; width: number; height: number};
