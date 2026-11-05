import type {ComponentType} from 'react';
import type {ColorValue, ImageSourcePropType, ViewProps} from 'react-native';
import '../godot';

export type GodotTextureProgressBarProps = ViewProps & {
  value?: number;
  minimumValue?: number;
  maximumValue?: number;
  step?: number;
  fillMode?:
    | 'leftToRight'
    | 'rightToLeft'
    | 'topToBottom'
    | 'bottomToTop'
    | 'clockwise'
    | 'counterClockwise'
    | 'bilinearLeftAndRight'
    | 'bilinearTopAndBottom'
    | 'clockwiseAndCounterClockwise';
  underTexture?: ImageSourcePropType;
  progressTexture?: ImageSourcePropType;
  overTexture?: ImageSourcePropType;
  underTint?: ColorValue;
  progressTint?: ColorValue;
  overTint?: ColorValue;
  radialInitialAngle?: number;
  radialFillDegrees?: number;
  radialCenterOffset?: {x: number; y: number};
  nineSliceStretch?: boolean;
  stretchMargins?: {left?: number; top?: number; right?: number; bottom?: number};
};

declare const GodotTextureProgressBar: ComponentType<GodotTextureProgressBarProps>;
export default GodotTextureProgressBar;
