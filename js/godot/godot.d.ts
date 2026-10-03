export type GodotColor = {
  $godot: 'Color';
  r: number;
  g: number;
  b: number;
  a: number;
};

export type GodotContextMenuEntry =
  | {id: string; label: string; disabled?: boolean; separator?: false}
  | {separator: true; label?: string};

export type GodotMouseEvent = {
  target: number;
  button: number;
  buttons: number;
  pageX: number;
  pageY: number;
  offsetX: number;
  offsetY: number;
};

declare module 'react-native' {
  interface ViewProps {
    godotContextMenu?: readonly GodotContextMenuEntry[];
    onGodotContextMenuAction?: (event: NativeSyntheticEvent<{id: string; target: number}>) => void;
    onMiddleClick?: (event: NativeSyntheticEvent<GodotMouseEvent>) => void;
    onMiddleClickCapture?: (event: NativeSyntheticEvent<GodotMouseEvent>) => void;
    onRightClick?: (event: NativeSyntheticEvent<GodotMouseEvent>) => void;
    onRightClickCapture?: (event: NativeSyntheticEvent<GodotMouseEvent>) => void;
  }
  interface PlatformStatic {
    OS: 'godot' | string;
  }
}
