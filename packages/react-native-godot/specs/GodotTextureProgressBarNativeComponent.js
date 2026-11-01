/**
 * @flow strict-local
 * @format
 */

import type {ColorValue} from 'react-native/Libraries/StyleSheet/StyleSheet';
import type {ImageSource} from 'react-native/Libraries/Image/ImageSource';
import type {ViewProps} from 'react-native/Libraries/Components/View/ViewPropTypes';
import type {Double, Int32, WithDefault} from 'react-native/Libraries/Types/CodegenTypes';
import type {HostComponent} from 'react-native/Libraries/Renderer/shims/ReactNativeTypes';

import codegenNativeComponent from 'react-native/Libraries/Utilities/codegenNativeComponent';

type NativeProps = $ReadOnly<{
  ...ViewProps,
  value?: Double,
  minimumValue?: Double,
  maximumValue?: WithDefault<Double, 100>,
  step?: Double,
  fillMode?: WithDefault<
    | 'leftToRight'
    | 'rightToLeft'
    | 'topToBottom'
    | 'bottomToTop'
    | 'clockwise'
    | 'counterClockwise'
    | 'bilinearLeftAndRight'
    | 'bilinearTopAndBottom'
    | 'clockwiseAndCounterClockwise',
    'leftToRight',
  >,
  underTexture?: ImageSource,
  progressTexture?: ImageSource,
  overTexture?: ImageSource,
  underTint?: ColorValue,
  progressTint?: ColorValue,
  overTint?: ColorValue,
  radialInitialAngle?: Double,
  radialFillDegrees?: WithDefault<Double, 360>,
  radialCenterOffsetX?: Double,
  radialCenterOffsetY?: Double,
  nineSliceStretch?: WithDefault<boolean, false>,
  stretchMarginLeft?: Int32,
  stretchMarginTop?: Int32,
  stretchMarginRight?: Int32,
  stretchMarginBottom?: Int32,
}>;

export default (codegenNativeComponent<NativeProps>('GodotTextureProgressBar'): HostComponent<NativeProps>);
