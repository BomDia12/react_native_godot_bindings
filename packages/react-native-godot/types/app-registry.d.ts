import type {ComponentType} from 'react';
import type {GodotAlertResult, GodotRegistrationOptions} from '../godot';

export type GodotAlert = (
  title?: string | null,
  message?: string | null,
  buttons?: readonly {text?: string; onPress?: () => void}[],
  options?: {cancelable?: boolean; onDismiss?: () => void},
) => Promise<GodotAlertResult>;

export const GodotAppRegistry: {
  registerComponent(key: string, provider: () => ComponentType<{rootTag: number}>, options?: GodotRegistrationOptions): string;
};
export function useGodotAlert(): GodotAlert;
export function useGodotRoot(): {rootTag: number; alert: GodotAlert} | null;
export default GodotAppRegistry;
