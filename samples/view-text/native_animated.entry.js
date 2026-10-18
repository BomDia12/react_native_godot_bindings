import React, {useState} from 'react';
import {Animated, AppRegistry, useAnimatedColor, useAnimatedValue, useAnimatedValueXY, View} from 'react-native';

global.__godotNativeAnimated = {finished: false, frameCallbacks: 0};

function NativeAnimatedApp() {
  const offset = useAnimatedValue(0);
  const position = useAnimatedValueXY({x: 0, y: 0});
  const tint = useAnimatedColor('#ff0000');
  const [color, setColor] = useState('#ff8800');
  global.__godotNativeAnimatedStart = () => {
    const requestAnimationFrame = global.requestAnimationFrame;
    global.requestAnimationFrame = callback => {
      global.__godotNativeAnimated.frameCallbacks += 1;
      return requestAnimationFrame(callback);
    };
    const timing = toValue => ({toValue, duration: 1500, useNativeDriver: true});
    Animated.parallel([
      Animated.timing(offset, timing(120)),
      Animated.timing(position, timing({x: 60, y: 30})),
      Animated.timing(tint, timing({r: 0, g: 0, b: 255, a: 1})),
    ]).start(({finished}) => {
      global.requestAnimationFrame = requestAnimationFrame;
      global.__godotNativeAnimated.finished = finished;
    });
  };
  global.__godotNativeAnimatedRecolor = () => setColor('#0088ff');

  return (
    <View style={{flex: 1}}>
      <Animated.View
        testID="native-animated-box"
        style={{width: 40, height: 40, backgroundColor: color, transform: [{translateX: offset}]}}
      />
      <Animated.View
        testID="native-animated-tinted"
        style={{width: 40, height: 40, backgroundColor: tint, transform: position.getTranslateTransform()}}
      />
    </View>
  );
}

AppRegistry.registerComponent('GodotNativeAnimatedApp', () => NativeAnimatedApp);
