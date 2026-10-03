'use strict';
import {callAsync} from './modules';

const roots = new Map();
let subscription;
const native = () => global.__godotNativeModules.get('GodotAlert');
const scope = (origin, callback) => global.__godotScheduler.withOrigin(origin, callback);
export function registerAlertRoot(rootTag, session, presenter) {
  const root = {session, origin: native().getOrigin(session), presenter};
  roots.set(rootTag, root);
  if (subscription == null) {
    subscription = global.__godotScheduler.withoutOrigin(() => native().onPresent(event => {
      const target = roots.get(event.rootTag);
      if (!target || typeof target.presenter !== 'function') { global.__godotNativeModules.cancel(event.requestId); return; }
      Promise.resolve().then(() => scope(target.origin, () => target.presenter(event.payload)))
        .then(result => native().reply(target.session, event.requestId, result))
        .catch(error => { global.__godotNativeModules.cancel(event.requestId); console.error(error); });
    }));
  }
  return () => {if (roots.get(rootTag) === root) {roots.delete(rootTag);}};
}
export function alertForOrigin(origin, title, message, buttons, options) {
  const callbacks = buttons?.length ? buttons : [{text: 'OK'}];
  const payload = {title: title ?? '', message: message ?? '', cancelable: options?.cancelable ?? false,
    buttons: callbacks.map(button => ({text: button.text ?? '', style: button.style ?? 'default'}))};
  const root = origin && roots.get(origin.rootTag);
  const presenter = root?.presenter;
  const mode = presenter === true || presenter === 'native' ? 'native' : typeof presenter === 'function' ? 'custom' : 'bubble';
  return callAsync(native(), 'request', [origin ?? null, payload, mode, root?.session ?? '']).then(result => {
    scope(origin ?? null, () => {
      if (result.dismissed) { options?.onDismiss?.(); }
      else { callbacks[result.buttonId]?.onPress?.(); }
    });
    return result;
  });
}
export function ambientAlert(...args) {
  return alertForOrigin(global.__godotScheduler.getOrigin(), ...args);
}
