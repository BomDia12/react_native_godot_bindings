export type GodotColor = {
  $godot: 'Color';
  r: number;
  g: number;
  b: number;
  a: number;
};

declare module 'react-native' {
  interface PlatformStatic {
    OS: 'godot' | string;
  }
}
