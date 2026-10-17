/**
 * @flow strict-local
 * @format
 */

import type {ViewProps} from 'react-native/Libraries/Components/View/ViewPropTypes';
import type {ColorValue} from 'react-native/Libraries/StyleSheet/StyleSheet';
import type {
  DirectEventHandler,
  Double,
} from 'react-native/Libraries/Types/CodegenTypes';
import type {HostComponent} from 'react-native/Libraries/Renderer/shims/ReactNativeTypes';

import * as React from 'react';
import codegenNativeCommands from 'react-native/Libraries/Utilities/codegenNativeCommands';
import codegenNativeComponent from 'react-native/Libraries/Utilities/codegenNativeComponent';

type ValueChangedEvent = $ReadOnly<{
  value: Double,
}>;

type NativeProps = $ReadOnly<{
  ...ViewProps,
  value?: Double,
  tint?: ?ColorValue,
  onValueChanged?: ?DirectEventHandler<ValueChangedEvent>,
}>;

type NativeType = HostComponent<NativeProps>;

interface NativeCommands {
  +advance: (viewRef: React.ElementRef<NativeType>, amount: Double) => void;
}

export const Commands: NativeCommands = codegenNativeCommands<NativeCommands>({
  supportedCommands: ['advance'],
});

export default (codegenNativeComponent<NativeProps>(
  'RNExampleMeter',
): NativeType);
