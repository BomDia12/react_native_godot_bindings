import React, {useEffect} from 'react';
import {AppRegistry, View} from 'react-native';
import {onGodotMessage, postGodotMessage, useGodotMessage, usePostGodotMessage} from 'react-native-godot/messages';
import {getGodotModule, useGodotModule} from 'react-native-godot/modules';

const communication = (global.__communication = {rootMessages: [], applicationMessages: [], moduleSnapshots: []});

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
  const module = useGodotModule('Scoreboard');
  if (module) {
    communication.moduleSnapshots.push(module.value);
  }
  return <View style={{flex: 1, backgroundColor: '#223344'}} />;
}

global.__moduleCall = (name, command, args) => getGodotModule(name).call(command, ...args);
global.__moduleCallAsync = (name, command, args) => getGodotModule(name).callAsync(command, ...args);
global.__moduleReady = name => getGodotModule(name).ready;

AppRegistry.registerComponent('GodotMessagesApp', () => MessagesApp);
AppRegistry.registerComponent('GodotModuleApp', () => ModuleApp);
