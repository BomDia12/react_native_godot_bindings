// Metro config schedules this import before InitializeCore.
import './godot.preamble';

import React from 'react';
import {AppRegistry, StyleSheet, Text, View} from 'react-native';

import './interaction.entry';
import './multi_root.entry';

const styles = StyleSheet.create({
  container: {
    flex: 1,
    padding: 16,
    backgroundColor: '#123456',
    opacity: 0.8,
  },
  sentinel: {
    color: '#00ff00',
    fontSize: 20,
  },
});

const App = () => (
  <View style={styles.container}>
    <Text style={styles.sentinel}>Baseline commit OK</Text>
  </View>
);

AppRegistry.registerComponent('GodotApp', () => App);

global.__godotBoundaryErrors = [];
global.__godotBoundaryCaught = false;

class SurfaceErrorBoundary extends React.Component {
  state = {failed: false};

  componentDidCatch(error, info) {
    global.__godotBoundaryCaught = true;
    global.__godotBoundaryErrors.push(this.props.rootTag);
    global.nativeFabricUIManager.__godotReportSurfaceError(
      this.props.rootTag,
      String(error?.message ?? error),
      info?.componentStack ?? '',
    );
    this.setState({failed: true});
  }

  render() {
    return this.state.failed ? null : this.props.children;
  }
}

AppRegistry.setWrapperComponentProvider(parameters => props => (
  <SurfaceErrorBoundary rootTag={parameters.rootTag} {...props} />
));

global.__godotRunApplication = (applicationKey, rootTag) =>
  AppRegistry.runApplication(applicationKey, {
    rootTag,
    initialProps: {},
    fabric: true,
  });

global.__godotStopApplication = rootTag => {
  global.RN$stopSurface(rootTag);
};
