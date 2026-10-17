'use strict';

import React, {createContext, useContext, useEffect, useMemo} from 'react';
import {AppRegistry} from 'react-native';
import NativeGodotAlert from '../specs/NativeGodotAlert';

const RootContext = createContext(null);
const presenters = new Map();
let presentations = null;

function presentThroughJavaScript({requestId, rootTag, payload}) {
  const presenter = presenters.get(rootTag);
  if (presenter == null) {
    NativeGodotAlert.decline(rootTag, requestId);
    return;
  }
  Promise.resolve()
    .then(() => presenter(payload))
    .then(
      result => {
        if (result?.handled === false) {
          NativeGodotAlert.decline(rootTag, requestId);
        } else {
          NativeGodotAlert.reply(rootTag, requestId, {buttonId: result?.buttonId ?? null, dismissed: Boolean(result?.dismissed)});
        }
      },
      error => {
        NativeGodotAlert.decline(rootTag, requestId);
        console.error(error);
      },
    );
}

function presentationMode(presenter) {
  if (presenter === true || presenter === 'native') {
    return 'native';
  }
  return typeof presenter === 'function' ? 'custom' : 'bubble';
}

function rootAlert(rootTag, mode) {
  return (title, message, buttons, options) => {
    const callbacks = buttons?.length ? buttons : [{text: 'OK'}];
    const payload = {
      title: title ?? '',
      message: message ?? '',
      cancelable: options?.cancelable ?? false,
      buttons: callbacks.map(button => ({text: button.text ?? '', style: button.style ?? 'default'})),
    };
    return NativeGodotAlert.request(rootTag, payload, mode).then(result => {
      if (result.dismissed) {
        options?.onDismiss?.();
      } else {
        callbacks[result.buttonId]?.onPress?.();
      }
      return result;
    });
  };
}

export const GodotAppRegistry = {
  registerComponent(appKey, provider, options = {}) {
    const Application = provider();
    const presenter = options.alerts ?? false;
    const mode = presentationMode(presenter);
    function RegisteredApplication(props) {
      const {rootTag} = props;
      useEffect(() => {
        if (mode !== 'custom') {
          return undefined;
        }
        presentations ??= NativeGodotAlert.onPresent(presentThroughJavaScript);
        presenters.set(rootTag, presenter);
        return () => presenters.delete(rootTag);
      }, [rootTag]);
      const root = useMemo(() => ({rootTag, alert: rootAlert(rootTag, mode)}), [rootTag]);
      return (
        <RootContext.Provider value={root}>
          <Application {...props} />
        </RootContext.Provider>
      );
    }
    return AppRegistry.registerComponent(appKey, () => RegisteredApplication, options.section ?? false);
  },
};

export function useGodotRoot() {
  return useContext(RootContext);
}

export function useGodotAlert() {
  const root = useGodotRoot();
  if (!root) {
    throw new Error('useGodotAlert requires GodotAppRegistry registration.');
  }
  return root.alert;
}

export default GodotAppRegistry;
