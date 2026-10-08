'use strict';
import * as NativeComponentRegistry from 'react-native/Libraries/NativeComponent/NativeComponentRegistry';
import {dispatchCommand} from 'react-native/Libraries/ReactNative/RendererProxy';
import {colorAttribute} from 'react-native/Libraries/Components/View/ReactNativeStyleAttributes';

export const NativeCommands = {
  focus: ref => dispatchCommand(ref, 'focus', []),
  blur: ref => dispatchCommand(ref, 'blur', []),
  setTextAndSelection: (ref, count, text, start, end) =>
    dispatchCommand(ref, 'setTextAndSelection', [count, text, start, end]),
};
const directEventTypes = {};
for (const name of ['Change', 'SelectionChange', 'SubmitEditing', 'EndEditing', 'Focus', 'Blur', 'KeyPress', 'ContentSizeChange']) {
  directEventTypes['top' + name] = {registrationName: 'on' + name};
}
export default NativeComponentRegistry.get('GodotTextInput', () => ({
  uiViewClassName: 'GodotTextInput',
  directEventTypes,
  validAttributes: {
    allowFontScaling: true, maxFontSizeMultiplier: true,
    text: true, multiline: true, mostRecentEventCount: true, selection: true,
    editable: true, readOnly: true, placeholder: true, secureTextEntry: true,
    submitBehavior: true, maxLength: true, autoFocus: true,
    selectionColor: colorAttribute, cursorColor: colorAttribute,
    placeholderTextColor: colorAttribute,
    ...Object.fromEntries(Object.keys(directEventTypes).map(key => [directEventTypes[key].registrationName, true])),
  },
}));
