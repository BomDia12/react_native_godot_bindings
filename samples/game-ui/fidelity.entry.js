import React, {useEffect, useRef, useState} from 'react';
import {Animated, AppRegistry, Easing, LayoutAnimation, ScrollView, Text, View} from 'react-native';
import {fontFamily} from 'react-native-godot/fonts';
import 'react-native-godot/setup';

const rendered = global.__fidelity = {};
const arabic = fontFamily(require('./assets/NotoSansArabic.ttf'));
const hebrew = fontFamily(require('./assets/NotoSansHebrew.ttf'));
const ink = '#003d5b';

function Screen({name, children, ready = true}) {
  useEffect(() => {
    rendered[name] = ready;
  }, [name, ready]);
  return <View style={{width: 480, height: 360, backgroundColor: '#f4f1ea', padding: 12, flexDirection: 'row', flexWrap: 'wrap', gap: 12}}>{children}</View>;
}

const tile = {width: 72, height: 72, backgroundColor: '#ffffff'};

function StyleGallery() {
  return <Screen name="StyleGallery">
    <View style={{...tile, borderTopWidth: 4, borderRightWidth: 6, borderBottomWidth: 8, borderLeftWidth: 2, borderTopColor: '#d1495b', borderRightColor: '#edae49', borderBottomColor: '#00798c', borderLeftColor: '#30638e'}} />
    <View style={{...tile, borderWidth: 3, borderColor: '#30638e', borderStyle: 'dashed'}} />
    <View style={{...tile, borderWidth: 3, borderColor: '#00798c', borderStyle: 'dotted'}} />
    <View style={{...tile, borderWidth: 2, borderColor: '#003d5b', borderTopLeftRadius: 24, borderTopRightRadius: 4, borderBottomRightRadius: 36, borderBottomLeftRadius: 12}} />
    <View style={{...tile, borderWidth: 2, borderColor: '#003d5b', borderRadius: '30%'}} />
    <View style={{...tile, borderRadius: 12, boxShadow: '4px 6px 8px 2px rgba(0, 0, 0, 0.35)'}} />
    <View style={{...tile, borderRadius: 12, boxShadow: 'inset 0px 4px 10px 4px rgba(209, 73, 91, 0.6)'}} />
    <View style={{...tile, borderRadius: 12, boxShadow: '0px 0px 0px 4px #edae49, 6px 6px 0px 0px #30638e'}} />
    <View style={{...tile, backgroundImage: 'linear-gradient(135deg, #d1495b, #edae49 50%, #00798c)'}} />
    <View style={{...tile, borderRadius: 36, backgroundImage: 'radial-gradient(circle at 30% 30%, #ffffff, #30638e)'}} />
    <View style={{...tile, outlineWidth: 3, outlineColor: '#d1495b', outlineOffset: 4, outlineStyle: 'dashed'}} />
    <View style={{...tile, transform: [{rotate: '15deg'}, {scale: 0.8}], transformOrigin: 'top left', backgroundColor: '#00798c'}} />
    <View style={{...tile, borderRadius: 18, overflow: 'hidden'}}>
      <ScrollView contentOffset={{x: 0, y: 24}}>
        <View style={{height: 48, backgroundColor: '#d1495b'}} />
        <View style={{height: 48, backgroundColor: '#edae49'}} />
      </ScrollView>
    </View>
    <View style={{...tile, backgroundColor: 'transparent', zIndex: 0}}>
      <View style={{position: 'absolute', left: 0, top: 0, width: 48, height: 48, backgroundColor: '#30638e', zIndex: 2}} />
      <View style={{position: 'absolute', left: 16, top: 16, width: 48, height: 48, backgroundColor: '#edae49', zIndex: 1}} />
      <View style={{position: 'absolute', left: 32, top: 32, width: 40, height: 40, backgroundColor: '#d1495b', zIndex: -1}} />
    </View>
    <View style={{...tile, justifyContent: 'center', alignItems: 'center'}}><Text style={{color: '#003d5b', fontSize: 14}}>Text</Text></View>
  </Screen>;
}

const swatch = {width: 40, height: 40};
const photo = {width: 40, height: 40, backgroundImage: 'linear-gradient(90deg, #d1495b, #edae49, #00798c, #30638e)'};
const filters = [
  [{brightness: 1.6}], [{contrast: 2}], [{grayscale: 1}], [{hueRotate: '90deg'}], [{invert: 1}],
  [{opacity: 0.4}], [{saturate: 3}], [{sepia: 1}], [{blur: 3}], [{dropShadow: '3px 3px 2px #003d5b'}],
  [{sepia: 1}, {hueRotate: '180deg'}, {blur: 1}],
];
const blendModes = ['normal', 'multiply', 'screen', 'overlay', 'darken', 'lighten', 'color-dodge', 'color-burn', 'hard-light', 'soft-light', 'difference', 'exclusion', 'hue', 'saturation', 'color', 'luminosity', 'plus-lighter'];
const backdrop = {position: 'absolute', left: 0, top: 0, width: 456, height: 104, backgroundImage: 'linear-gradient(90deg, #003d5b, #f4f1ea 50%, #d1495b)'};

function EffectsGallery() {
  return <Screen name="EffectsGallery">
    {filters.map((filter, index) => <View key={'f' + index} style={{...photo, filter}} />)}
    <View style={{width: 456, height: 104}}>
      <View style={backdrop} />
      <View style={{flexDirection: 'row', flexWrap: 'wrap', gap: 4, padding: 4}}>
        {blendModes.map(mode => <View key={mode} style={{...swatch, backgroundColor: '#edae49', borderRadius: 8, mixBlendMode: mode}} />)}
      </View>
    </View>
    <View style={{width: 96, height: 72}}>
      <View style={{position: 'absolute', left: 0, top: 0, width: 56, height: 56, backgroundColor: '#00798c', mixBlendMode: 'multiply'}} />
      <View style={{position: 'absolute', left: 28, top: 16, width: 56, height: 56, backgroundColor: '#edae49', mixBlendMode: 'multiply'}} />
    </View>
    <View style={{width: 96, height: 72, backgroundColor: '#30638e', isolation: 'isolate'}}>
      <View style={{margin: 12, width: 56, height: 40, backgroundColor: '#edae49', mixBlendMode: 'difference'}} />
    </View>
    <View style={{width: 72, height: 72, borderRadius: 24, overflow: 'hidden', backgroundColor: '#ffffff'}}>
      <View style={{...photo, width: 72, height: 72, filter: [{hueRotate: '120deg'}]}} />
    </View>
    <View style={{width: 72, height: 72, filter: [{grayscale: 1}]}}>
      <View style={{width: 72, height: 72, borderRadius: 36, overflow: 'hidden'}}>
        <View style={{...photo, width: 72, height: 72}} />
      </View>
    </View>
    <View style={{...photo, opacity: 0.5, filter: [{hueRotate: '180deg'}]}} />
    <View style={{width: 56, height: 56, backgroundColor: '#30638e', filter: [{saturate: 2}]}}>
      <View style={{margin: 8, width: 40, height: 40, backgroundColor: '#edae49', mixBlendMode: 'difference'}} />
    </View>
    <View style={{width: 40, height: 40, backgroundColor: '#00798c', filter: [{dropShadow: '4px 4px 0px #d1495b'}, {blur: 2}]}} />
  </Screen>;
}

function RTLGallery() {
  return <Screen name="RTLGallery">
    <View style={{width: 456, height: 48, flexDirection: 'row', gap: 8}}>
      {['#d1495b', '#edae49', '#00798c', '#30638e'].map(color => <View key={color} style={{width: 48, height: 48, backgroundColor: color}} />)}
    </View>
    <View style={{width: 456, height: 56, backgroundColor: '#ffffff', borderStartWidth: 8, borderStartColor: '#d1495b', borderEndWidth: 2, borderEndColor: '#30638e', marginStart: 24, borderTopStartRadius: 16}} />
    <View style={{width: 456, height: 72, backgroundColor: '#ffffff'}}>
      <View style={{position: 'absolute', start: 8, top: 8, width: 56, height: 56, backgroundColor: '#00798c'}} />
      <View style={{position: 'absolute', end: 8, top: 8, width: 56, height: 56, backgroundColor: '#edae49'}} />
      <View style={{position: 'absolute', left: 120, top: 8, width: 56, height: 56, backgroundColor: '#30638e'}} />
    </View>
    <Text style={{width: 456, color: ink, fontSize: 14}}>Natural alignment follows the layout</Text>
    <Text style={{width: 456, color: ink, fontSize: 14, textAlign: 'left'}}>Left swaps to the right</Text>
    <Text style={{width: 456, color: ink, fontSize: 14, fontFamily: hebrew}}>שלום עולם</Text>
    <Text style={{width: 456, color: ink, fontSize: 14, fontFamily: arabic}}>مرحبا بالعالم</Text>
  </Screen>;
}

const line = {width: 220, color: ink, fontSize: 14};
const long = 'The quick brown fox jumps over the lazy dog';

function TextGallery() {
  return <Screen name="TextGallery">
    <View style={{width: 220, gap: 4}}>
      <Text style={line}>Regular <Text style={{fontWeight: 'bold'}}>bold</Text> <Text style={{fontStyle: 'italic'}}>italic</Text> <Text style={{letterSpacing: 3}}>spaced</Text></Text>
      <Text style={{...line, textDecorationLine: 'underline', textDecorationStyle: 'double', textDecorationColor: '#d1495b'}}>Double underline</Text>
      <Text style={{...line, textDecorationLine: 'underline line-through', textDecorationStyle: 'dashed'}}>Dashed both lines</Text>
      <Text style={{...line, textDecorationLine: 'underline', textDecorationStyle: 'dotted', textDecorationColor: '#00798c'}}>Dotted underline</Text>
      <Text style={{...line, textShadowColor: 'rgba(209, 73, 91, 0.8)', textShadowOffset: {width: 2, height: 2}, textShadowRadius: 1}}>Text shadow</Text>
      <Text style={line}><Text style={{textTransform: 'uppercase'}}>upper</Text> <Text style={{textTransform: 'capitalize'}}>capital letters</Text> <Text style={{textTransform: 'lowercase'}}>LOWER</Text></Text>
      <Text style={{...line, fontVariant: ['tabular-nums']}}>1111 | 8888</Text>
      <Text style={{...line, lineHeight: 28, backgroundColor: '#ffffff'}}>Line height 28{'\n'}second line</Text>
      <Text style={{...line, borderWidth: 2, borderColor: '#d1495b', padding: 4}}>Bordered and padded</Text>
      <Text style={line}>Mixed <Text style={{fontFamily: hebrew}}>שלום עולם</Text> and <Text style={{fontFamily: arabic}}>مرحبا</Text></Text>
    </View>
    <View style={{width: 220, gap: 6}}>
      <Text numberOfLines={1} ellipsizeMode="tail" style={line}>{long}</Text>
      <Text numberOfLines={1} ellipsizeMode="head" style={line}>{long}</Text>
      <Text numberOfLines={1} ellipsizeMode="middle" style={line}>{long}</Text>
      <Text numberOfLines={1} ellipsizeMode="clip" style={line}>{long}</Text>
      <Text numberOfLines={2} style={line}>{long + ' ' + long}</Text>
      <Text numberOfLines={1} adjustsFontSizeToFit style={{...line, fontSize: 20}}>{long}</Text>
      <Text style={{...line, textAlign: 'center'}}>Centered</Text>
      <Text style={{...line, textAlign: 'right'}}>Right</Text>
      <Text style={{...line, textAlign: 'justify'}}>Justified text spreads its words over the whole width of the line.</Text>
      <Text style={{...line, fontFamily: arabic, writingDirection: 'rtl'}}>مرحبا بالعالم من محرك جودو</Text>
    </View>
  </Screen>;
}

AppRegistry.registerComponent('TextGallery', () => TextGallery);
// Every animation runs to its end; the golden records where each one stops.
function AnimationGallery() {
  const js = useRef(new Animated.Value(0)).current;
  const native = useRef(new Animated.Value(0)).current;
  const spring = useRef(new Animated.Value(0)).current;
  const [expanded, setExpanded] = useState(false);
  const [done, setDone] = useState(false);
  useEffect(() => {
    LayoutAnimation.configureNext({...LayoutAnimation.Presets.easeInEaseOut, duration: 300});
    setExpanded(true);
    Animated.parallel([
      Animated.timing(js, {toValue: 1, duration: 300, easing: Easing.inOut(Easing.quad), useNativeDriver: false}),
      Animated.timing(native, {toValue: 1, duration: 300, easing: Easing.out(Easing.cubic), useNativeDriver: true}),
      Animated.spring(spring, {toValue: 1, stiffness: 180, damping: 18, useNativeDriver: true}),
    ]).start(() => setTimeout(() => setDone(true), 400));
  }, []);
  const tile = {width: 72, height: 72, margin: 12};
  return <Screen name="AnimationGallery" ready={done}>
    <Animated.View style={{...tile, opacity: js.interpolate({inputRange: [0, 1], outputRange: [0.2, 1]}), backgroundColor: js.interpolate({inputRange: [0, 1], outputRange: ['#d1495b', '#00798c']}), transform: [{translateY: js.interpolate({inputRange: [0, 1], outputRange: [0, 40]})}]}} />
    <Animated.View style={{...tile, backgroundColor: '#30638e', opacity: native, transform: [{rotate: native.interpolate({inputRange: [0, 1], outputRange: ['0deg', '45deg']})}, {scale: native.interpolate({inputRange: [0, 1], outputRange: [0.5, 1]})}]}} />
    <Animated.View style={{...tile, backgroundColor: '#edae49', transform: [{translateX: spring.interpolate({inputRange: [0, 1], outputRange: [0, 60]})}]}} />
    <View style={{width: expanded ? 300 : 72, height: 48, margin: 12, backgroundColor: '#003d5b'}} />
    <Animated.Text style={{color: js.interpolate({inputRange: [0, 1], outputRange: ['#d1495b', '#003d5b']}), fontSize: 18, margin: 12}}>Animated text</Animated.Text>
  </Screen>;
}

AppRegistry.registerComponent('AnimationGallery', () => AnimationGallery);
AppRegistry.registerComponent('StyleGallery', () => StyleGallery);
AppRegistry.registerComponent('EffectsGallery', () => EffectsGallery);
AppRegistry.registerComponent('RTLGallery', () => RTLGallery);
