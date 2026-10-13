'use strict';
const crypto = require('crypto');
const adaptations = {
  'Libraries/vendor/emitter/EventEmitter.js': source => source.replace('    const registrations = allocate<', `    const origin = global.__godotScheduler?.getOrigin?.() ?? null;
    if (global.__godotScheduler?.withOrigin != null) {
      const callback = listener;
      listener = (...args) => global.__godotScheduler.withOrigin(origin, () => callback.apply(context, args));
    }
    const registrations = allocate<`),
  'Libraries/Components/Touchable/TouchableOpacity.js': source => source.replace('useNativeDriver: true,', "useNativeDriver: Platform.OS !== 'godot',"),
  'src/private/animated/createAnimatedPropsHook.js': source => source.replace("import NativeAnimatedHelper from './NativeAnimatedHelper';", "import NativeAnimatedHelper from './NativeAnimatedHelper';\nimport Platform from '../../../Libraries/Utilities/Platform';").replace('if (!NativeAnimatedHelper.shouldSignalBatch) {', "if (Platform.OS !== 'godot' && !NativeAnimatedHelper.shouldSignalBatch) {"),
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
