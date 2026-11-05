/**
 * @flow strict-local
 * @format
 */

import type {ViewProps} from 'react-native/Libraries/Components/View/ViewPropTypes';
import type {Double, DirectEventHandler, WithDefault} from 'react-native/Libraries/Types/CodegenTypes';
import type {HostComponent} from 'react-native/Libraries/Renderer/shims/ReactNativeTypes';

import * as React from 'react';
import codegenNativeCommands from 'react-native/Libraries/Utilities/codegenNativeCommands';
import codegenNativeComponent from 'react-native/Libraries/Utilities/codegenNativeComponent';

type ErrorEvent = $ReadOnly<{
  message: string,
}>;

type NativeProps = $ReadOnly<{
  ...ViewProps,
  source: string,
  autoplay?: WithDefault<boolean, false>,
  paused?: WithDefault<boolean, false>,
  loop?: WithDefault<boolean, false>,
  volume?: WithDefault<Double, 1>,
  bus?: WithDefault<string, 'Master'>,
  expand?: WithDefault<boolean, true>,
  onLoad?: ?DirectEventHandler<null>,
  onEnd?: ?DirectEventHandler<null>,
  onError?: ?DirectEventHandler<ErrorEvent>,
}>;

type NativeType = HostComponent<NativeProps>;

interface NativeCommands {
  +play: (viewRef: React.ElementRef<NativeType>) => void;
  +seek: (viewRef: React.ElementRef<NativeType>, seconds: Double) => void;
}

export const Commands: NativeCommands = codegenNativeCommands<NativeCommands>({
  supportedCommands: ['play', 'seek'],
});

export default (codegenNativeComponent<NativeProps>('GodotVideo'): NativeType);
