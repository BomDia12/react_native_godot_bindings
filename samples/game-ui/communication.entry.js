import React, {useContext, useEffect} from 'react';
import {AppRegistry, View} from 'react-native';
import {RootTagContext} from 'react-native/Libraries/ReactNative/RootTag';
import {onGodotMessage, postGodotMessage, useGodotMessage, usePostGodotMessage} from 'react-native-godot/messages';
import {getGodotModule} from 'react-native-godot/modules';
import {useScoreboard} from 'react-native-godot-script-example';

const communication = (global.__communication = {rootMessages: [], applicationMessages: [], scores: [], scoreboards: {}, asyncScores: []});

// Application messages come back with the same payload under another name.
onGodotMessage('broadcast', payload => {
  communication.applicationMessages.push(payload);
  postGodotMessage('broadcast-ack', payload);
});

function MessagesApp() {
  const post = usePostGodotMessage();
  useGodotMessage('ping', payload => {
    communication.rootMessages.push(payload);
    post('pong', payload);
  });
  useEffect(() => {
    communication.messagesReady = true;
  }, []);
  return <View style={{flex: 1}} />;
}

function ModuleApp() {
  const rootTag = useContext(RootTagContext);
  const scoreboard = useScoreboard();
  if (scoreboard) {
    communication.scores.push(scoreboard.score);
    communication.scoreboards[rootTag] = scoreboard;
  }
  return <View style={{flex: 1, backgroundColor: '#223344'}} />;
}

global.__scoreboardAdd = (rootTag, points) => communication.scoreboards[rootTag].add(points);
global.__scoreboardAddLater = (rootTag, points) => {
  communication.scoreboards[rootTag].addLater(points).then(score => communication.asyncScores.push(score));
};
global.__moduleReady = name => getGodotModule(name).ready;
global.__postObjectMessage = () => {
  try {
    postGodotMessage('object', {node: {$godot: 'Object', handle: '1'}});
    return null;
  } catch (error) {
    return error.code;
  }
};

AppRegistry.registerComponent('GodotMessagesApp', () => MessagesApp);
AppRegistry.registerComponent('GodotModuleApp', () => ModuleApp);
