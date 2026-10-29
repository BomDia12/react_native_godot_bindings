import type {GodotColor, GodotInt64, GodotRect2, GodotVector2, GodotVector3} from '../godot';

export const int64: (value: bigint | number | string) => GodotInt64;
export const color: (r: number, g: number, b: number, a?: number) => GodotColor;
export const vector2: (x: number, y: number) => GodotVector2;
export const vector3: (x: number, y: number, z: number) => GodotVector3;
export const rect2: (x: number, y: number, width: number, height: number) => GodotRect2;
