'use strict';
import React, {createContext, useContext, useEffect, useState} from 'react';
import {AppRegistry} from 'react-native';
import {openSession, closeSession} from './modules';
import {registerAlertRoot, alertForOrigin} from './alerts';

const RootContext = createContext(null);
export const GodotAppRegistry = {
  registerComponent(appKey, provider, options = {}) {
    const Application = provider();
    function RegisteredApplication(props) {
      const [root, setRoot] = useState(null);
      useEffect(() => {
        const session = openSession(props.rootTag);
        const origin = global.__godotNativeModules.get('GodotAlert').getOrigin(session);
        const unregister = registerAlertRoot(props.rootTag, session, options.alerts ?? false);
        setRoot({session, origin, alert: (...args) => alertForOrigin(origin, ...args)});
        return () => {unregister(); closeSession(session);};
      }, [props.rootTag]);
      if (root == null) {return null;}
      return <RootContext.Provider value={root}><Application {...props} /></RootContext.Provider>;
    }
    return AppRegistry.registerComponent(appKey, () => RegisteredApplication, options.section ?? false);
  },
};
export function useGodotRoot() { return useContext(RootContext); }
export function useGodotAlert() {
  const root = useGodotRoot();
  if (!root) { throw new Error('useGodotAlert requires GodotAppRegistry registration.'); }
  return root.alert;
}
export default GodotAppRegistry;
