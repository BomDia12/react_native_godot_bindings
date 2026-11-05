/**
 * @flow strict-local
 * @format
 */

import type {ViewProps} from 'react-native/Libraries/Components/View/ViewPropTypes';
import type {Double, WithDefault} from 'react-native/Libraries/Types/CodegenTypes';
import type {HostComponent} from 'react-native/Libraries/Renderer/shims/ReactNativeTypes';

import codegenNativeComponent from 'react-native/Libraries/Utilities/codegenNativeComponent';

type NativeProps = $ReadOnly<{
  ...ViewProps,
  value?: Double,
  minimumValue?: Double,
  maximumValue?: WithDefault<Double, 100>,
  step?: Double,
  fillMode?: WithDefault<'beginToEnd' | 'endToBegin' | 'topToBottom' | 'bottomToTop', 'beginToEnd'>,
  showPercentage?: WithDefault<boolean, false>,
}>;

export default (codegenNativeComponent<NativeProps>('GodotProgressBar', {
  interfaceOnly: true,
}): HostComponent<NativeProps>);
