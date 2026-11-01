import React from 'react';
import {AppRegistry, Text} from 'react-native';

global.__segments.push('startup');

AppRegistry.registerComponent('StartupSegmentApp', () => () => <Text>Startup segment</Text>);
