import type {ComponentType} from 'react';
import type {NativeSyntheticEvent, ViewProps} from 'react-native';
import '../godot';

export type GodotViewportViewProps = ViewProps & {
  // A name given to ReactNativeHost.register_viewport.
  viewport: string;
  resizeMode?: 'cover' | 'contain' | 'stretch' | 'center' | 'repeat';
  // Keeps the viewport's size at the view's size in pixels.
  matchSize?: boolean;
  // Sends pointer and key input on to the viewport.
  forwardInput?: boolean;
  onLoad?: (event: NativeSyntheticEvent<null>) => void;
  onError?: (event: NativeSyntheticEvent<{message: string}>) => void;
};

declare const GodotViewportView: ComponentType<GodotViewportViewProps>;
export default GodotViewportView;
