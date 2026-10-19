import type {GodotSceneSnapshot} from '../godot';

export type GodotSceneChange = {
  rootTag: number;
  binding: string | null;
  ready: boolean;
  sequence: number;
  event: string;
  payload?: unknown;
};

export function useGodotScene<T = unknown>(rootTag: number): GodotSceneSnapshot<T> | null;
export function getBinding(rootTag: number): {ready: boolean; binding: string | null; sequence: number; capability?: string; schemaVersion?: number};
export function read<T = unknown>(rootTag: number, handle: string): {payload: T; sequence: number};
export function call<T = unknown>(rootTag: number, handle: string, command: string, args?: readonly unknown[]): T;
export function callAsync<T = unknown>(rootTag: number, handle: string, command: string, args?: readonly unknown[]): Promise<T>;
export function onChanged(rootTag: number, callback: (change: GodotSceneChange) => void): {remove(): void};
