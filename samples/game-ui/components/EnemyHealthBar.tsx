import {StyleSheet, Text, View} from 'react-native';
import type {EnemyHealthBarProps} from '../godot/contracts';

declare const global: {__godotContracts?: Record<string, unknown>};

export default function EnemyHealthBar({displayName, health, maxHealth, marked, tint}: EnemyHealthBarProps) {
  global.__godotContracts = {...global.__godotContracts, EnemyHealthBar: {displayName, health, maxHealth, marked, tint}};
  const color = `rgba(${tint.r * 255}, ${tint.g * 255}, ${tint.b * 255}, ${tint.a})`;
  return (
    <View style={styles.bar}>
      <Text>{`${marked ? '* ' : ''}${displayName} ${health}/${maxHealth}`}</Text>
      <View style={[styles.fill, {width: `${(100 * health) / Math.max(maxHealth, 1)}%`, backgroundColor: color}]} />
    </View>
  );
}

const styles = StyleSheet.create({
  bar: {
    flex: 1,
    padding: 4,
  },
  fill: {
    height: 6,
  },
});
