import type {ComponentType} from 'react';
import type {ViewProps} from 'react-native';
import '../godot';

export type GodotProgressBarProps = ViewProps & {
  value?: number;
  minimumValue?: number;
  maximumValue?: number;
  step?: number;
  fillMode?: 'beginToEnd' | 'endToBegin' | 'topToBottom' | 'bottomToTop';
  showPercentage?: boolean;
};

declare const GodotProgressBar: ComponentType<GodotProgressBarProps>;
export default GodotProgressBar;
