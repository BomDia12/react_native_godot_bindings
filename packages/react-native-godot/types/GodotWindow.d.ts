import type {ComponentType} from 'react';
import type {NativeSyntheticEvent, ViewProps} from 'react-native';
import '../godot';

export type GodotWindowProps = ViewProps & {
  title?: string;
  width?: number;
  height?: number;
  visible?: boolean;
  onShow?: (event: NativeSyntheticEvent<null>) => void;
  onDismiss?: (event: NativeSyntheticEvent<null>) => void;
  onRequestClose?: (event: NativeSyntheticEvent<null>) => void;
  onResize?: (event: NativeSyntheticEvent<{width: number; height: number}>) => void;
};

declare const GodotWindow: ComponentType<GodotWindowProps>;
export default GodotWindow;
