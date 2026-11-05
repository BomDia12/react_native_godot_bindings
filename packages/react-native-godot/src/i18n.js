'use strict';

import {useEffect, useMemo, useState} from 'react';
import NativeGodotTranslation from '../specs/NativeGodotTranslation';

// Each string crosses to the main thread once per locale, up to a limit that keeps
// counters and interpolated text from growing the cache for as long as a locale lasts.
const MAX_CACHED_TRANSLATIONS = 2048;
let translations = new Map();

// Subscribed once for the whole bundle, so nothing reads old strings when GDScript
// changes the locale while no component is listening.
NativeGodotTranslation.onLocaleChanged(() => {
  translations = new Map();
});

function cached(key, translate) {
  let value = translations.get(key);
  if (value === undefined) {
    value = translate();
    if (translations.size >= MAX_CACHED_TRANSLATIONS) {
      translations = new Map();
    }
    translations.set(key, value);
  }
  return value;
}

export function tr(message, context = '') {
  return cached(`t\u0000${context}\u0000${message}`, () => NativeGodotTranslation.translate(message, context));
}

export function trN(message, plural, n, context = '') {
  return cached(`p\u0000${context}\u0000${message}\u0000${plural}\u0000${n}`, () => NativeGodotTranslation.translatePlural(message, plural, n, context));
}

export const getLocale = () => NativeGodotTranslation.getLocale();
export const getLoadedLocales = () => NativeGodotTranslation.getLoadedLocales();

export function setLocale(locale) {
  NativeGodotTranslation.setLocale(locale);
  translations = new Map();
}

export function onLocaleChanged(handler) {
  return NativeGodotTranslation.onLocaleChanged(handler);
}

export function useTranslation() {
  const [locale, setCurrentLocale] = useState(getLocale);
  // Godot also reports new catalogs under an unchanged locale, which should rerender.
  const [revision, setRevision] = useState(0);
  useEffect(() => {
    setCurrentLocale(getLocale());
    const subscription = onLocaleChanged(next => {
      setCurrentLocale(next);
      setRevision(current => current + 1);
    });
    return () => subscription.remove();
  }, []);
  // eslint-disable-next-line react-hooks/exhaustive-deps
  return useMemo(() => ({tr: (...args) => tr(...args), trN: (...args) => trN(...args), locale}), [locale, revision]);
}
