import React from 'react';
import {AppRegistry, Text} from 'react-native';

global.__segments = [];

AppRegistry.registerComponent('SegmentHostApp', () => () => <Text>Segment host</Text>);
