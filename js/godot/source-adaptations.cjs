'use strict';
const crypto = require('crypto');
const adaptations = {
  'Libraries/Components/Touchable/TouchableOpacity.js': source => source.replace('useNativeDriver: true,', "useNativeDriver: Platform.OS !== 'godot',"),
  'src/private/animated/createAnimatedPropsHook.js': source => source.replace("import NativeAnimatedHelper from './NativeAnimatedHelper';", "import NativeAnimatedHelper from './NativeAnimatedHelper';\nimport Platform from '../../../Libraries/Utilities/Platform';").replace('if (!NativeAnimatedHelper.shouldSignalBatch) {', "if (Platform.OS !== 'godot' && !NativeAnimatedHelper.shouldSignalBatch) {"),
  'Libraries/Modal/Modal.js': source => source.replace(
    "  _shouldShowModal(): boolean {\n    if (Platform.OS === 'ios') {",
    "  _shouldShowModal(): boolean {\n    if (Platform.OS === 'ios' || Platform.OS === 'godot') {",
  ).replace(
    "    const onDismiss = () => {\n      // OnDismiss is implemented on iOS only.\n      if (Platform.OS === 'ios') {",
    "    const onDismiss = () => {\n      if (Platform.OS === 'ios' || Platform.OS === 'godot') {",
  ),
  'Libraries/Components/TextInput/TextInput.js': source => {
    source = source.replace("} else if (Platform.OS === 'ios') {", `} else if (Platform.OS === 'godot') {
  const native = require('react-native-godot/TextInputHost');
  RCTSinglelineTextInputView = native.default;
  RCTMultilineTextInputView = native.default;
  RCTSinglelineTextInputNativeCommands = native.NativeCommands;
  RCTMultilineTextInputNativeCommands = native.NativeCommands;
} else if (Platform.OS === 'ios') {`);
    source = source.replace("  if (Platform.OS === 'ios') {", "  if (Platform.OS === 'ios' || Platform.OS === 'godot') {");
    return source.replace('<RCTTextInputView\n', '<RCTTextInputView\n        multiline={multiline}\n');
  },
  'Libraries/Components/TextInput/TextInputState.js': source =>
    source.replaceAll("if (Platform.OS === 'ios') {", "if (Platform.OS === 'ios' || Platform.OS === 'godot') {"),
  'Libraries/Components/ScrollView/ScrollView.js': source => {
    source = source.replace('  _updateAnimatedNodeAttachment() {', `  _updateAnimatedNodeAttachment() {
    if (Platform.OS === 'godot') {
      this._scrollAnimatedValueAttachment?.detach();
      this._scrollAnimatedValueAttachment = null;
      return;
    }`);
    return source.replace('  _handleScroll = (e: ScrollEvent) => {', `  _handleScroll = (e: ScrollEvent) => {
    if (Platform.OS === 'godot' && this.props.stickyHeaderIndices?.length) {
      this._scrollAnimatedValue.setValue(e.nativeEvent.contentOffset.y);
    }`);
  },
};
const hashes = require('./source-adaptations.json');
function adaptSource(filename, source) {
  const normalized = filename.replaceAll('\\', '/');
  for (const [relative, adapt] of Object.entries(adaptations)) {
    if (normalized.endsWith('/react-native/' + relative)) {
      const digest = crypto.createHash('sha256').update(source).digest('hex');
      if (digest !== hashes[relative]) {
        throw new Error('Godot source adaptation requires pinned RN 0.87.1: ' + relative);
      }
      return adapt(source);
    }
  }
  return source;
}
module.exports = {adaptSource, adaptations};
