export type Scoreboard = {
  score: number;
  add(points: number): number;
  addLater(points: number): Promise<number>;
};

export function useScoreboard(): Scoreboard | null;
