import React, {useRef} from 'react';
import {AppRegistry, PanResponder, View} from 'react-native';

const fixture = (global.__godotMultiTouch = {left: [], right: []});

const touchIds = event => event.nativeEvent.touches.map(touch => touch.identifier).sort();

// The left root records raw touch events only: a responder there would keep
// the right root's PanResponder from taking the touch, because upstream
// negotiates the responder from the common ancestor of both views and views
// in different roots have none.
function TouchRoot({side, responds}) {
  const events = fixture[side];
  const record = (type, details) => events.push({type, ...details});
  const responder = useRef(
    PanResponder.create({
      onStartShouldSetPanResponder: () => true,
      onPanResponderGrant: (_, gesture) => record('grant', {active: gesture.numberActiveTouches}),
      onPanResponderMove: (_, gesture) => record('move', {active: gesture.numberActiveTouches}),
      onPanResponderRelease: () => record('release', {}),
      onPanResponderTerminate: () => record('terminate', {}),
    }),
  ).current;
  return (
    <View
      nativeID="touch_target"
      {...(responds ? responder.panHandlers : {})}
      onTouchStart={event => record('touchStart', {touches: touchIds(event)})}
      onTouchMove={event => record('touchMove', {touches: touchIds(event)})}
      onTouchCancel={event => record('touchCancel', {touches: touchIds(event)})}
      onPointerDown={event => record('pointerDown', {primary: event.nativeEvent.isPrimary})}
      onPointerCancel={() => record('pointerCancel', {})}
      style={{flex: 1, backgroundColor: '#335577'}}
    />
  );
}

AppRegistry.registerComponent('GodotTouchLeftApp', () => () => <TouchRoot side="left" responds={false} />);
AppRegistry.registerComponent('GodotTouchRightApp', () => () => <TouchRoot side="right" responds />);
