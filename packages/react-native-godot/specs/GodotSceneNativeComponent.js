/**
 * @flow strict-local
 * @format
 */

import type {ViewProps} from 'react-native/Libraries/Components/View/ViewPropTypes';
import type {DirectEventHandler} from 'react-native/Libraries/Types/CodegenTypes';
import type {HostComponent} from 'react-native/Libraries/Renderer/shims/ReactNativeTypes';

import codegenNativeComponent from 'react-native/Libraries/Utilities/codegenNativeComponent';

type LoadErrorEvent = $ReadOnly<{
  message: string,
}>;

type NativeProps = $ReadOnly<{
  ...ViewProps,
  source: string,
  onLoad?: ?DirectEventHandler<$ReadOnly<{}>>,
  onError?: ?DirectEventHandler<LoadErrorEvent>,
}>;

export default (codegenNativeComponent<NativeProps>('GodotScene'): HostComponent<NativeProps>);
