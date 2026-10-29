import type {ComponentType} from 'react';

export function withGodotContract<P extends object>(Component: ComponentType<P>, events?: Record<string, string>): ComponentType<{rootTag: number}>;
