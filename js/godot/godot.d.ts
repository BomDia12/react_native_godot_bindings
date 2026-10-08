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

export type GodotRootOrigin = {generation: number; rootTag: number; epoch: number};
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
  session: {$godot: 'Session'; handle: string};
  handle: string;
  value: T;
  sequence: number;
};

// Applications can augment these modules with their own snapshot and command types.
declare module 'react-native-godot/app-registry' {
  export const GodotAppRegistry: {
    registerComponent(key: string, provider: () => React.ComponentType<{rootTag: number}>, options?: GodotRegistrationOptions): string;
  };
  export function useGodotAlert(): (title?: string | null, message?: string | null, buttons?: readonly {text?: string; onPress?: () => void}[], options?: {cancelable?: boolean; onDismiss?: () => void}) => Promise<GodotAlertResult>;
  export function useGodotRoot(): {session: GodotSceneSnapshot<unknown>['session']; origin: GodotRootOrigin; alert: ReturnType<typeof useGodotAlert>} | null;
}
declare module 'react-native-godot/scene' {
  export function useGodotScene<T = unknown>(rootTag: number): GodotSceneSnapshot<T> | null;
  export function getBinding(session: GodotSceneSnapshot<unknown>['session']): {ready: boolean; binding: string | null; sequence: number};
  export function read<T = unknown>(session: GodotSceneSnapshot<T>['session'], handle: string): {payload: T; sequence: number};
  export function call<T = unknown>(session: GodotSceneSnapshot<unknown>['session'], handle: string, command: string, args?: readonly unknown[]): T;
  export function callAsync<T = unknown>(session: GodotSceneSnapshot<unknown>['session'], handle: string, command: string, args?: readonly unknown[], options?: {signal?: AbortSignal}): Promise<T>;
  export function onChanged(session: GodotSceneSnapshot<unknown>['session'], callback: (event: {ready: boolean; sequence: number; event: string; payload: unknown}) => void): {remove(): void};
}
