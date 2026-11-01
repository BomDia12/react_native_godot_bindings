export type GodotInputEvent =
  | {type: 'key'; physicalKeycode: number; keycode: number; modifiers: GodotInputModifiers; label: string}
  | {type: 'mouseButton'; button: number; modifiers: GodotInputModifiers; label: string}
  | {type: 'joypadButton'; device: number; button: number; label: string}
  | {type: 'joypadMotion'; device: number; axis: number; axisValue: number; label: string};
export type GodotInputModifiers = {alt: boolean; ctrl: boolean; shift: boolean; meta: boolean};
export type GodotInputDevice = 'keyboard' | 'mouse' | 'joypad' | 'touch';
export type GodotActionEvent = {action: string; pressed: boolean; strength: number; echo: boolean};
export type GodotJoypad = {device: number; name: string};
type Subscription = {remove(): void};

export const GodotInput: {
  getActions(): string[];
  getActionEvents(action: string): GodotInputEvent[];
  setActionEvents(action: string, events: readonly GodotInputEvent[]): void;
  resetAction(action: string): void;
  captureNextEvent(options?: {devices?: readonly GodotInputDevice[]}): Promise<GodotInputEvent>;
  cancelCapture(): void;
  getLastDevice(): GodotInputDevice;
  getJoypads(): GodotJoypad[];
  vibrateJoypad(device: number, options?: {weak?: number; strong?: number; durationMs?: number}): void;
  stopJoypadVibration(device: number): void;
  onAction(action: string, handler: (event: GodotActionEvent) => void): Subscription;
  onLastDeviceChanged(handler: (device: GodotInputDevice) => void): Subscription;
  onJoypadsChanged(handler: (joypads: GodotJoypad[]) => void): Subscription;
};
export function useInputAction(action: string, handler: (event: GodotActionEvent) => void): void;
export function useLastInputDevice(): GodotInputDevice;
