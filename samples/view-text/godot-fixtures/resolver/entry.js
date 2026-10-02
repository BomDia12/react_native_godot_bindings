import Platform from 'react-native/Libraries/Utilities/Platform';
import BaseViewConfig from 'react-native/Libraries/NativeComponent/BaseViewConfig';
import js from './selected-js';
import jsx from './selected-jsx';
import ts from './selected-ts';
import tsx from './selected-tsx';
import nativeFallback from './native-fallback';
import genericFallback from './generic-fallback';

global.__godotResolverFixture = {
  os: Platform.OS,
  selectedUndefined: Platform.select({
    godot: undefined,
    native: 'wrong-native',
    default: 'wrong-default',
  }),
  js,
  jsx,
  ts,
  tsx,
  nativeFallback,
  genericFallback,
  pointerEnterSkipsBubbling:
    BaseViewConfig.bubblingEventTypes.topPointerEnter.phasedRegistrationNames
      .skipBubbling === true,
  pointerLeaveSkipsBubbling:
    BaseViewConfig.bubblingEventTypes.topPointerLeave.phasedRegistrationNames
      .skipBubbling === true,
};
