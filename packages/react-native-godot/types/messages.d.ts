export function postGodotMessage(name: string, payload?: unknown, rootTag?: number | null): void;
export function onGodotMessage<T = unknown>(name: string, handler: (payload: T) => void, rootTag?: number | null): {remove(): void};
export function useGodotMessage<T = unknown>(name: string, handler: (payload: T) => void): void;
export function usePostGodotMessage(): (name: string, payload?: unknown) => void;
