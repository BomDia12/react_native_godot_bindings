import {useGodotModule} from 'react-native-godot/modules';

export function useScoreboard() {
  const scoreboard = useGodotModule('Scoreboard');
  if (scoreboard == null) {
    return null;
  }
  return {
    score: scoreboard.value.score,
    add: points => scoreboard.module.call('add', points),
    addLater: points => scoreboard.module.callAsync('add_later', points),
  };
}
