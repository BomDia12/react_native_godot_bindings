/**
 * @flow strict-local
 * @format
 */

import type {ViewProps} from 'react-native/Libraries/Components/View/ViewPropTypes';
import type {DirectEventHandler, WithDefault} from 'react-native/Libraries/Types/CodegenTypes';
import type {HostComponent} from 'react-native/Libraries/Renderer/shims/ReactNativeTypes';

import codegenNativeComponent from 'react-native/Libraries/Utilities/codegenNativeComponent';

type ErrorEvent = $ReadOnly<{
  message: string,
}>;

type NativeProps = $ReadOnly<{
  ...ViewProps,
  viewport: string,
  resizeMode?: WithDefault<'cover' | 'contain' | 'stretch' | 'center' | 'repeat', 'cover'>,
  matchSize?: WithDefault<boolean, false>,
  forwardInput?: WithDefault<boolean, false>,
  onLoad?: ?DirectEventHandler<null>,
  onError?: ?DirectEventHandler<ErrorEvent>,
}>;

export default (codegenNativeComponent<NativeProps>('GodotViewportView'): HostComponent<NativeProps>);
