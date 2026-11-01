import React from 'react';
import {AppRegistry, Text} from 'react-native';

global.__segments.push('runtime');

AppRegistry.registerComponent('RuntimeSegmentApp', () => () => <Text>Runtime segment</Text>);
