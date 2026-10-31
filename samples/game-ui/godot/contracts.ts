// Generated from the Godot component contracts in res://contracts. Do not edit.
import type {GodotColor} from 'react-native-godot';

export type RosterEnemy = { health: number; id: string; maxHealth: number; name: string; };

export type EnemyHealthBarProps = {
  displayName: string;
  health: number;
  maxHealth: number;
  marked: boolean;
  tint: GodotColor;
};

export type EnemyRosterProps = {
  enemies: readonly RosterEnemy[];
  marked: string | null;
  onMark: (enemyId: string) => void;
  onHeal: (enemyId: string, amount: number) => void;
};

export type GodotContracts = {
  EnemyHealthBar: EnemyHealthBarProps;
  EnemyRoster: EnemyRosterProps;
};
