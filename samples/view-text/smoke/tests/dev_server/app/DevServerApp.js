import React, {useEffect, useState} from 'react';
import {LogBox, Text, View} from 'react-native';

global.__godotLogBox = LogBox;

export default function DevServerApp() {
  const [count, setCount] = useState(0);
  useEffect(() => {
    global.__godotDevServerBump = () => setCount(value => value + 1);
    return () => {
      delete global.__godotDevServerBump;
    };
  }, []);
  return (
    <View style={{flex: 1}}>
      <Text>{`Fast Refresh before ${count}`}</Text>
    </View>
  );
}
