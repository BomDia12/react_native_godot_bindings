/**
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 *
 * @flow
 * @format
 */

import type {HostComponent} from 'react-native/src/private/types/HostComponent';
import type {ProcessedColorValue} from 'react-native/Libraries/StyleSheet/processColor';
import type {GestureResponderEvent} from 'react-native/Libraries/Types/CoreEventTypes';
import type {TextProps} from 'react-native/Libraries/Text/TextProps';

import * as React from 'react';
import {enablePreparedTextLayout} from 'react-native/src/private/featureflags/ReactNativeFeatureFlags';
import {createViewConfig} from 'react-native/Libraries/NativeComponent/ViewConfig';
import UIManager from 'react-native/Libraries/ReactNative/UIManager';
import createReactNativeComponentClass from 'react-native/Libraries/Renderer/shims/createReactNativeComponentClass'
import flattenStyle from 'react-native/Libraries/StyleSheet/flattenStyle';

export type NativeTextProps = Readonly<{
  ...TextProps,
  isHighlighted?: ?boolean,
  selectionColor?: ?ProcessedColorValue,
  onClick?: ?(event: GestureResponderEvent) => unknown,
  // This is only needed for platforms that optimize text hit testing, e.g.,
  // react-native-windows. It can be used to only hit test virtual text spans
  // that have pressable events attached to them.
  isPressable?: ?boolean,
}>;

const textViewConfig = {
  validAttributes: {
    isHighlighted: true,
    isPressable: true,
    numberOfLines: true,
    ellipsizeMode: true,
    allowFontScaling: true,
    dynamicTypeRamp: true,
    maxFontSizeMultiplier: true,
    disabled: true,
    selectable: true,
    selectionColor: true,
    adjustsFontSizeToFit: true,
    minimumFontScale: true,
    textBreakStrategy: true,
    onTextLayout: true,
    dataDetectorType: true,
    android_hyphenationFrequency: true,
    lineBreakStrategyIOS: true,
  },
  directEventTypes: {
    topTextLayout: {
      registrationName: 'onTextLayout',
    },
  },
  uiViewClassName: 'RCTText',
} as const;

const virtualTextViewConfig = {
  validAttributes: {
    isHighlighted: true,
    isPressable: true,
    maxFontSizeMultiplier: true,
  },
  uiViewClassName: 'RCTVirtualText',
} as const;

// Spans inherit text attributes and nothing else from their paragraph, so the
// theme type travels as the fontFamily that the text shaper reads back.
function withGodotThemeType(HostText: any): any {
  return function GodotText({godotThemeType, style, ...props}: any) {
    if (godotThemeType == null) {
      return <HostText {...props} style={style} />;
    }
    const family = flattenStyle(style)?.fontFamily ?? '';
    return (
      <HostText
        {...props}
        style={[style, {fontFamily: `godot-theme:${godotThemeType}|${family}`}]}
      />
    );
  };
}

/**
 * `NativeText` is an internal React Native host component, and is exported to
 * provide lower-level access for libraries.
 *
 * @warning `<unstable_NativeText>` provides no semver guarantees and is not
 *   intended to be used in app code. Please use
 *   [`<Text>`](https://reactnative.dev/docs/text) instead.
 */
// Additional note: Our long term plan is to reduce the overhead of the <Text>
// and <View> wrappers so that we no longer have any reason to export these APIs.
export const NativeText: HostComponent<NativeTextProps> =
  withGodotThemeType(
    createReactNativeComponentClass('RCTText', () =>
      createViewConfig(textViewConfig),
    ),
  );

export const NativeVirtualText: HostComponent<NativeTextProps> =
  !global.RN$Bridgeless && !UIManager.hasViewManagerConfig('RCTVirtualText')
    ? NativeText
    : withGodotThemeType(
        createReactNativeComponentClass('RCTVirtualText', () =>
          createViewConfig(virtualTextViewConfig),
        ),
      );

export const NativeSelectableText: HostComponent<NativeTextProps> =
  enablePreparedTextLayout()
    ? withGodotThemeType(
        createReactNativeComponentClass('RCTSelectableText', () =>
          createViewConfig({
            ...textViewConfig,
            uiViewClassName: 'RCTSelectableText',
          }),
        ),
      )
    : NativeText;
