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

type MetaEvent = $ReadOnly<{
  meta: string,
}>;

type NativeProps = $ReadOnly<{
  ...ViewProps,
  bbcode?: string,
  customEffects?: $ReadOnlyArray<string>,
  visibleRatio?: WithDefault<Double, 1>,
  visibleCharactersBehavior?: WithDefault<'charsBeforeShaping' | 'charsAfterShaping' | 'glyphsLayoutDirection' | 'glyphsLeftToRight' | 'glyphsRightToLeft', 'charsBeforeShaping'>,
  selectionEnabled?: WithDefault<boolean, false>,
  scrollActive?: WithDefault<boolean, false>,
  onMetaClick?: ?DirectEventHandler<MetaEvent>,
  onMetaHoverStarted?: ?DirectEventHandler<MetaEvent>,
  onMetaHoverEnded?: ?DirectEventHandler<MetaEvent>,
  onRevealFinished?: ?DirectEventHandler<null>,
}>;

type NativeType = HostComponent<NativeProps>;

interface NativeCommands {
  +reveal: (viewRef: React.ElementRef<NativeType>, toRatio: Double, seconds: Double) => void;
}

export const Commands: NativeCommands = codegenNativeCommands<NativeCommands>({
  supportedCommands: ['reveal'],
});

export default (codegenNativeComponent<NativeProps>('GodotRichText', {
  interfaceOnly: true,
}): NativeType);
