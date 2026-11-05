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
    // A Theme type variation of Panel, whose StyleBox replaces the background and borders.
    godotThemeType?: string;
    // A StyleBox resource, which replaces the background and borders.
    godotStyleBox?: string;
    // A ShaderMaterial resource that shades this view and its children.
    godotMaterial?: string;
    godotShaderParameters?: Record<string, number | boolean | GodotColor | GodotVector2 | GodotVector3>;
    tooltip?: string;
    // What a drag from this view carries. Godot reads it when the drag starts.
    godotDragData?: {type: string; [key: string]: unknown};
    godotDragPreview?: ImageSourcePropType;
    // The drag types this view accepts, matched against the drag data's `type`.
    godotDropTypes?: readonly string[];
    onGodotDrop?: (event: NativeSyntheticEvent<{data: {type: string; [key: string]: unknown}; locationX: number; locationY: number; pageX: number; pageY: number}>) => void;
    onGodotDragEnd?: (event: NativeSyntheticEvent<{successful: boolean; pageX: number; pageY: number}>) => void;
    onGodotContextMenuAction?: (event: NativeSyntheticEvent<{id: string; target: number}>) => void;
    onMiddleClick?: (event: NativeSyntheticEvent<GodotMouseEvent>) => void;
    onMiddleClickCapture?: (event: NativeSyntheticEvent<GodotMouseEvent>) => void;
    onRightClick?: (event: NativeSyntheticEvent<GodotMouseEvent>) => void;
    onRightClickCapture?: (event: NativeSyntheticEvent<GodotMouseEvent>) => void;
  }
  interface TextProps {
    // A Theme type variation of RichTextLabel, which sets the font, size and color
    // that the style doesn't.
    godotThemeType?: string;
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
