export type GodotRootState = {
  visible: boolean;
  processing: boolean;
  focused: boolean;
};

export function getRootState(rootTag: number): GodotRootState;
export function onRootStateChanged(rootTag: number, callback: (state: GodotRootState) => void): {remove(): void};
export function useGodotRootState(): GodotRootState;
