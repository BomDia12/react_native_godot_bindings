// Generated from the Godot component contracts in res://contracts. Do not edit.
import {GodotAppRegistry} from 'react-native-godot/app-registry';
import {withGodotContract} from 'react-native-godot/contracts';
import EnemyHealthBar from '../components/EnemyHealthBar';
import EnemyRoster from '../components/EnemyRoster';

GodotAppRegistry.registerComponent('EnemyHealthBar', () => withGodotContract(EnemyHealthBar, {}));
GodotAppRegistry.registerComponent('EnemyRoster', () => withGodotContract(EnemyRoster, {mark: 'onMark', heal: 'onHeal'}));
