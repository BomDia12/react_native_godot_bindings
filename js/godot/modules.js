'use strict';

const registry = global.__godotNativeModules;

export const openSession = rootTag => registry.openSession(rootTag);
export const closeSession = session => registry.closeSession(session);

export function callAsync(module, method, args, options = {}) {
  if (module == null || typeof module.__godotStartAsync !== 'function') {
    throw new TypeError(
      'GodotModules.callAsync expects a native Godot module proxy.',
    );
  }
  if (module.__godotNativeModuleBrand !== true) {
    throw new TypeError('GodotModules.callAsync rejects unbranded module objects.');
  }
  if (options.signal?.aborted) {
    const error = new Error('Godot native call was cancelled before start.');
    error.code = 'E_CANCELLED';
    return Promise.reject(error);
  }
  const {requestId, promise} = module.__godotStartAsync(method, args);
  const abort = () => registry.cancel(requestId);
  options.signal?.addEventListener('abort', abort, {once: true});
  return promise.finally(() =>
    options.signal?.removeEventListener('abort', abort),
  );
}
