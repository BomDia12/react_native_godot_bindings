import {GodotAppRegistry} from 'react-native-godot/app-registry';
import App from './App';
import './godot/registry';

GodotAppRegistry.registerComponent('App', () => App);
