import React, {useEffect, useRef, useState} from 'react';
import {AppRegistry, StyleSheet, Text, View} from 'react-native';
import * as NativeComponentRegistry from 'react-native/Libraries/NativeComponent/NativeComponentRegistry';
import RNExampleMeter, {Commands} from 'react-native-godot/specs/RNExampleMeterNativeComponent';

const MissingFixture = NativeComponentRegistry.get('RNMissingFixture', () => ({
  uiViewClassName: 'RNMissingFixture',
  validAttributes: {},
}));

function ComponentRegistryApp() {
  const meter = useRef(null);
  const [eventValue, setEventValue] = useState(-1);
  useEffect(() => {
    const timer = setTimeout(() => {
      Commands.advance(meter.current, 0.25);
      meter.current?.measure((x, y, width, height) => {
        global.__godotComponentRegistryState.measure = {x, y, width, height};
      });
    }, 0);
    return () => clearTimeout(timer);
  }, []);
  global.__godotComponentRegistryState = {
    eventValue,
    registered: global.__nativeComponentRegistry__hasComponent('RNExampleMeter'),
    unknownRejected: !global.__nativeComponentRegistry__hasComponent('RNMissingFixture'),
    measure: global.__godotComponentRegistryState?.measure ?? null,
  };
  return (
    <View style={styles.container}>
      <RNExampleMeter
        ref={meter}
        style={styles.meter}
        value={0.25}
        tint="#33cc66"
        onValueChanged={event => setEventValue(event.nativeEvent.value)}
      />
      <MissingFixture style={styles.missing} />
      <Text>{`Meter ${eventValue}`}</Text>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {width: 120, height: 80},
  meter: {width: 80, height: 20},
  missing: {width: 10, height: 10},
});
AppRegistry.registerComponent('GodotComponentRegistryApp', () => ComponentRegistryApp);
