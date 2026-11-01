'use strict';

import {useEffect, useMemo, useState} from 'react';
import NativeGodotTranslation from '../specs/NativeGodotTranslation';

// Each string crosses to the main thread once per locale.
let translations = new Map();

function cached(key, translate) {
  let value = translations.get(key);
  if (value === undefined) {
    value = translate();
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
  return NativeGodotTranslation.onLocaleChanged(locale => {
    translations = new Map();
    handler(locale);
  });
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
