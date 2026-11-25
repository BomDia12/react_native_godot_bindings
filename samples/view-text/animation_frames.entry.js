import React from 'react';
import {Animated, AppRegistry, View} from 'react-native';

const fixture = (global.__godotFrames = {});

fixture.count = duration => {
  const result = (fixture.counted = {done: false, callbacks: 0});
  const start = performance.now();
  const step = () => {
    result.callbacks += 1;
    if (performance.now() - start < duration) {
      requestAnimationFrame(step);
    } else {
      result.done = true;
    }
  };
  requestAnimationFrame(step);
};

fixture.order = () => {
  const result = (fixture.ordered = {done: false, timestamps: [], nested: null, cancelledRan: false});
  let cancelledDuringFrame = 0;
  requestAnimationFrame(timestamp => {
    result.timestamps.push(timestamp);
    cancelAnimationFrame(cancelledDuringFrame);
    requestAnimationFrame(nested => {
      result.nested = nested;
      result.done = true;
    });
  });
  requestAnimationFrame(timestamp => result.timestamps.push(timestamp));
  const cancelled = requestAnimationFrame(() => {
    result.cancelledRan = true;
  });
  cancelledDuringFrame = requestAnimationFrame(() => {
    result.cancelledRan = true;
  });
  requestAnimationFrame(timestamp => result.timestamps.push(timestamp));
  cancelAnimationFrame(cancelled);
};

// Each callback records when it ran. A callback that keeps JavaScript busy
// lets the Godot frame clock tick many times before the next callbacks run.
fixture.busy = () => {
  const result = (fixture.stalled = {done: false, runs: []});
  let busy = true;
  const step = () => {
    result.runs.push(performance.now());
    if (busy) {
      busy = false;
      const end = performance.now() + 200;
      while (performance.now() < end) {}
    }
    if (result.runs.length < 4) {
      requestAnimationFrame(step);
    } else {
      result.done = true;
    }
  };
  requestAnimationFrame(step);
};

fixture.decay = () => {
  const result = (fixture.decayed = {done: false, value: 0});
  const value = new Animated.Value(0);
  value.addListener(({value: current}) => {
    result.value = current;
  });
  // Upstream's decay ends on an update that moves it less than 0.1, and it
  // measures time with Date.now(), so it starts on a frame boundary rather
  // than just before a frame that is already due.
  requestAnimationFrame(() =>
    Animated.decay(value, {velocity: 1, deceleration: 0.998, useNativeDriver: false}).start(() => {
      result.done = true;
    }),
  );
};

fixture.throwing = () => {
  const result = (fixture.thrown = {done: false});
  requestAnimationFrame(() => {
    throw new Error('intentional frame callback failure');
  });
  requestAnimationFrame(() => {
    result.done = true;
  });
};

function AnimationFramesApp() {
  return <View style={{flex: 1, backgroundColor: '#223344'}} />;
}

AppRegistry.registerComponent('GodotAnimationFramesApp', () => AnimationFramesApp);
