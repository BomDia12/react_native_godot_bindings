import {Pressable, Text, View} from 'react-native';
import type {EnemyRosterProps} from '../godot/contracts';

declare const global: {__godotContracts?: Record<string, unknown>; __godotRosterEvents?: Pick<EnemyRosterProps, 'onMark' | 'onHeal'>};

export default function EnemyRoster({enemies, marked, onMark, onHeal}: EnemyRosterProps) {
  global.__godotContracts = {...global.__godotContracts, EnemyRoster: {enemies, marked}};
  global.__godotRosterEvents = {onMark, onHeal};
  return (
    <View>
      {enemies.map(enemy => (
        <Pressable key={enemy.id} onPress={() => onMark(enemy.id)}>
          <Text>{`${enemy.id === marked ? '* ' : ''}${enemy.name} ${enemy.health}/${enemy.maxHealth}`}</Text>
        </Pressable>
      ))}
    </View>
  );
}
