/**
 * @flow strict-local
 * @format
 */

import type {ViewProps} from 'react-native/Libraries/Components/View/ViewPropTypes';
import type {
  DirectEventHandler,
  Double,
  WithDefault,
} from 'react-native/Libraries/Types/CodegenTypes';
import type {HostComponent} from 'react-native/Libraries/Renderer/shims/ReactNativeTypes';

import codegenNativeComponent from 'react-native/Libraries/Utilities/codegenNativeComponent';

type ResizeEvent = $ReadOnly<{
  width: Double,
  height: Double,
}>;

type NativeProps = $ReadOnly<{
  ...ViewProps,
  title?: WithDefault<string, 'React Native'>,
  windowWidth?: WithDefault<Double, 640>,
  windowHeight?: WithDefault<Double, 480>,
  visible?: WithDefault<boolean, true>,
  onShow?: ?DirectEventHandler<null>,
  onDismiss?: ?DirectEventHandler<null>,
  onRequestClose?: ?DirectEventHandler<null>,
  onResize?: ?DirectEventHandler<ResizeEvent>,
}>;

export default (codegenNativeComponent<NativeProps>(
  'GodotWindow',
): HostComponent<NativeProps>);
