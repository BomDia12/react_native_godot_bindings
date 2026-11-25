import type {Component, ComponentType, ElementRef, RefAttributes} from 'react';
import type {NativeSyntheticEvent, ViewProps} from 'react-native';
import '../godot';

export type GodotVideoProps = ViewProps & {
  // A res:// path to a VideoStream, or a required asset.
  source: string | number;
  autoplay?: boolean;
  paused?: boolean;
  loop?: boolean;
  volume?: number;
  bus?: string;
  expand?: boolean;
  onLoad?: (event: NativeSyntheticEvent<null>) => void;
  onEnd?: (event: NativeSyntheticEvent<null>) => void;
  onError?: (event: NativeSyntheticEvent<{message: string}>) => void;
};

declare const GodotVideo: ComponentType<GodotVideoProps & RefAttributes<any>>;
export default GodotVideo;
export const GodotVideoCommands: {
  play(view: ElementRef<typeof Component>): void;
  seek(view: ElementRef<typeof Component>, seconds: number): void;
};
