import type {ComponentType} from 'react';
import type {NativeSyntheticEvent, ViewProps} from 'react-native';
import '../godot';

export type GodotSceneProps = ViewProps & {
  // A res:// path to a PackedScene.
  source: string;
  onLoad?: (event: NativeSyntheticEvent<{}>) => void;
  onError?: (event: NativeSyntheticEvent<{message: string}>) => void;
};

declare const GodotScene: ComponentType<GodotSceneProps>;
export default GodotScene;
