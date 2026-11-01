export function tr(message: string, context?: string): string;
export function trN(message: string, plural: string, n: number, context?: string): string;
export function getLocale(): string;
export function getLoadedLocales(): string[];
export function setLocale(locale: string): void;
export function onLocaleChanged(handler: (locale: string) => void): {remove(): void};
export function useTranslation(): {tr: typeof tr; trN: typeof trN; locale: string};
