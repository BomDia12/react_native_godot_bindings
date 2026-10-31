import type {GodotSceneChange} from './scene';

export type GodotModule = {
  ready: boolean;
  read<T = unknown>(): {payload: T; sequence: number};
  call<T = unknown>(command: string, ...args: unknown[]): T;
  callAsync<T = unknown>(command: string, ...args: unknown[]): Promise<T>;
  onChanged(callback: (change: GodotSceneChange) => void): {remove(): void};
};

export function getGodotModule(name: string): GodotModule;
export function onModuleChanged(name: string, callback: (change: GodotSceneChange) => void): {remove(): void};
export function useGodotModule<T = unknown>(name: string): {module: GodotModule; value: T; sequence: number} | null;
