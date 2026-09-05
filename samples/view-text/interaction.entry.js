import './godot.preamble';

import React, {useRef, useState} from 'react';
import {AppRegistry, Pressable, StyleSheet, Text, View} from 'react-native';

const log = [];
const layouts = [];
let lastRenderedCount = -1;
global.__godotStopPropagation = false;

const priorityConstants = {
  default: global.nativeFabricUIManager.unstable_DefaultEventPriority,
  discrete: global.nativeFabricUIManager.unstable_DiscreteEventPriority,
  continuous: global.nativeFabricUIManager.unstable_ContinuousEventPriority,
  idle: global.nativeFabricUIManager.unstable_IdleEventPriority,
};
const outsidePriority =
  global.nativeFabricUIManager.unstable_getCurrentEventPriority();

const record = name => {
  log.push(name);
};

const recordPriority = name => {
  const priority = global.nativeFabricUIManager.unstable_getCurrentEventPriority();
  record(`priority:${name}:${priority}`);
};

function App() {
  const [count, setCount] = useState(0);
  const [pressed, setPressed] = useState(false);
  const counterRef = useRef(null);
  if (count !== lastRenderedCount) {
    lastRenderedCount = count;
    record(`render-count:${count}`);
  }

  global.__godotInteractionState = {
    count,
    pressed,
    log,
    layouts,
    priorityConstants,
    outsidePriority,
  };

  return (
    <View
      style={styles.outer}
      onPointerDownCapture={() => record('outer-capture')}
      onPointerDown={() => record('outer-bubble')}>
      <Pressable
        ref={counterRef}
        focusable
        onLayout={event => {
          layouts.push(event.nativeEvent.layout);
          record('layout');
          recordPriority('layout');
        }}
        onPointerDownCapture={() => record('inner-capture')}
        onPointerDown={event => {
          record('inner-bubble');
          recordPriority('pointer-down');
          if (global.__godotStopPropagation) {
            event.stopPropagation();
          }
        }}
        onPointerMove={() => recordPriority('pointer-move')}
        onTouchStart={event =>
          record(
            `touch-shape:${event.nativeEvent.touches.length}:${event.nativeEvent.changedTouches.length}`,
          )
        }
        onKeyDown={event =>
          record(
            `key-down:${event.nativeEvent.key}:${event.nativeEvent.code}:${event.nativeEvent.repeat}`,
          )
        }
        onKeyUp={event =>
          record(
            `key-up:${event.nativeEvent.key}:${event.nativeEvent.code}:${event.nativeEvent.repeat}`,
          )
        }
        onPressIn={() => {
          record('press-in');
          counterRef.current?.measure((x, y, width, height, pageX, pageY) =>
            record(`measure:${x}:${y}:${width}:${height}:${pageX}:${pageY}`),
          );
          setPressed(true);
        }}
        onPressOut={() => {
          record('press-out');
          setPressed(false);
        }}
        onPress={() => {
          record('press');
          setCount(value => value + 1);
        }}
        onHoverIn={() => record('hover-in')}
        onHoverOut={() => record('hover-out')}
        onFocus={() => record('counter-focus')}
        onBlur={() => record('counter-blur')}
        style={[
          styles.counter,
          count > 0 ? styles.wide : null,
          count === 1 ? styles.outlined : null,
          pressed ? styles.pressed : count > 0 ? styles.changed : null,
        ]}>
        <Text style={styles.text}>{`Count ${count}`}</Text>
      </Pressable>
      <Pressable
        focusable
        onFocus={() => record('secondary-focus')}
        onBlur={() => record('secondary-blur')}
        style={styles.secondary}>
        <Text style={styles.text}>Secondary</Text>
      </Pressable>
    </View>
  );
}

const styles = StyleSheet.create({
  outer: {
    flex: 1,
    padding: 2,
    backgroundColor: '#111827',
  },
  counter: {
    width: 40,
    height: 20,
    backgroundColor: '#335577',
    padding: 2,
  },
  pressed: {
    backgroundColor: '#aa5500',
  },
  wide: {
    width: 42,
  },
  changed: {
    backgroundColor: '#227744',
  },
  // Applied at exactly one count, so the following commit removes these props
  // instead of changing them.
  outlined: {
    borderWidth: 2,
    borderColor: '#ffffff',
  },
  secondary: {
    width: 30,
    height: 15,
    marginTop: 3,
    backgroundColor: '#773355',
    padding: 1,
  },
  text: {
    color: '#ffffff',
    fontSize: 8,
  },
});

AppRegistry.registerComponent('GodotInteractionApp', () => App);

global.__godotRunApplication = (applicationKey, rootTag) =>
  AppRegistry.runApplication(applicationKey, {
    rootTag,
    initialProps: {},
    fabric: true,
  });

global.__godotStopApplication = rootTag => {
  global.RN$stopSurface(rootTag);
};
