export type GodotColor = {
  $godot: 'Color';
  r: number;
  g: number;
  b: number;
  a: number;
};

export type GodotVector2 = {$godot: 'Vector2'; x: number; y: number};
export type GodotVector3 = {$godot: 'Vector3'; x: number; y: number; z: number};
export type GodotRect2 = {$godot: 'Rect2'; x: number; y: number; width: number; height: number};
export type GodotTransform2D = {$godot: 'Transform2D'; xx: number; xy: number; yx: number; yy: number; ox: number; oy: number};
export type GodotInt64 = {$godot: 'int64'; value: string};
export type GodotObject = {$godot: 'Object'; handle: string};

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

export type GodotAlertResult =
  | {buttonId: number; dismissed: false}
  | {buttonId: null; dismissed: true};
export type GodotAlertPayload = {
  title: string;
  message: string;
  buttons: readonly {text: string}[];
  cancelable: boolean;
};
export type GodotRegistrationOptions = {
  section?: boolean;
  alerts?: boolean | ((payload: GodotAlertPayload) => GodotAlertResult | {handled: false} | Promise<GodotAlertResult | {handled: false}>);
};
export type GodotSceneSnapshot<T> = {
  rootTag: number;
  handle: string;
  value: T;
  sequence: number;
};
