import React, {useEffect, useRef, useState} from 'react';
import {
  AppRegistry,
  findNodeHandle,
  requireNativeComponent,
  StyleSheet,
  Text,
  UIManager,
  View,
} from 'react-native';

const RNExampleMeter = requireNativeComponent('RNExampleMeter');

function DescriptorApp() {
  const meter = useRef(null);
  const [value, setValue] = useState(0.25);
  const [eventValue, setEventValue] = useState(-1);
  useEffect(() => {
    let timer = setTimeout(() => {
      timer = setTimeout(() => {
        const tag = findNodeHandle(meter.current);
        UIManager.dispatchViewManagerCommand(tag, 'advance', [0.25]);
        meter.current?.measure((x, y, width, height) => {
          global.__godotDescriptorState.measure = {x, y, width, height};
        });
      }, 0);
    }, 0);
    return () => clearTimeout(timer);
  }, []);
  global.__godotDescriptorState = {
    value,
    eventValue,
    registered: global.__nativeComponentRegistry__hasComponent('RNExampleMeter'),
    unknownRejected: !global.__nativeComponentRegistry__hasComponent('RNMissingFixture'),
    measure: global.__godotDescriptorState?.measure ?? null,
  };
  return (
    <View style={styles.container}>
      <RNExampleMeter
        ref={meter}
        style={styles.meter}
        value={value}
        tint="#33cc66"
        onValueChanged={event => {
          setEventValue(event.nativeEvent.value);
          setValue(current => current);
        }}
      />
      <Text>{`Meter ${eventValue}`}</Text>
    </View>
  );
}

const styles = StyleSheet.create({
  container: {width: 120, height: 60},
  meter: {alignSelf: 'flex-start'},
});
AppRegistry.registerComponent('GodotDescriptorApp', () => DescriptorApp);
