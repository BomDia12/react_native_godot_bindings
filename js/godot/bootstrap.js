'use strict';

global.RN$Bridgeless = true;
global.RN$useAlwaysAvailableJSErrorHandling = true;
global.__fbBatchedBridgeConfig = {remoteModuleConfig: []};

const CALLABLE_MODULES = {};
global.RN$registerCallableModule = (name, moduleOrFactory) => {
  CALLABLE_MODULES[name] = moduleOrFactory;
};
global.__godotCallableModules = CALLABLE_MODULES;

const hostDescriptors = global.__godotHostDescriptors;
global.__nativeComponentRegistry__hasComponent = name =>
  Boolean(hostDescriptors?.hasComponent(name));
global.RN$LegacyInterop_UIManager_getConstants = () => {
  const metadata = hostDescriptors?.getConstants() ?? {};
  return {
    ...(metadata.ViewManagerConfigs ?? {}),
    genericBubblingEventTypes: metadata.genericBubblingEventTypes ?? {},
    genericDirectEventTypes: metadata.genericDirectEventTypes ?? {},
  };
};
global.RN$LegacyInterop_UIManager_getConstantsForViewManager = name =>
  hostDescriptors?.getConfig(name) ?? null;

const TIMERS = new Map();
let nextTimerId = 1;
function schedule(fn, delayMs, repeatMs, args) {
  const id = nextTimerId++;
  TIMERS.set(id, {fn, args, due: Date.now() + (delayMs || 0), repeatMs});
  return id;
}
const enqueueImmediate = (fn, ...args) => schedule(fn, 0, null, args);
global.setTimeout = (fn, ms, ...args) => schedule(fn, ms, null, args);
global.setInterval = (fn, ms, ...args) => schedule(fn, ms, ms || 0, args);
global.setImmediate = enqueueImmediate;
global.requestAnimationFrame = fn => schedule(() => fn(Date.now()), 0, null, []);
global.clearTimeout = id => TIMERS.delete(id);
global.clearInterval = global.clearTimeout;
global.clearImmediate = global.clearTimeout;
global.cancelAnimationFrame = global.clearTimeout;
global.__godotFlushTimers = () => {
  const now = Date.now();
  Array.from(TIMERS.keys()).forEach(id => {
    const timer = TIMERS.get(id);
    if (timer.due > now) {
	  return;
    }
    if (timer.repeatMs == null) {
      TIMERS.delete(id);
    } else {
      timer.due = now + timer.repeatMs;
    }
    timer.fn(...timer.args);
	});
};

const unsupported = operation => {
  const error = new Error(operation + ' is pending on the Godot platform.');
  error.code = 'E_UNSUPPORTED';
  error.operation = operation;
  throw error;
};

const SHIMS = {
  DeviceInfo: {
    getConstants: () => ({
      Dimensions: {
        window: {width: 800, height: 600, scale: 1, fontScale: 1},
        screen: {width: 800, height: 600, scale: 1, fontScale: 1},
      },
    }),
  },
  SourceCode: {getConstants: () => ({scriptURL: null})},
  NativeMicrotasksCxx: {
    queueMicrotask: callback =>
      typeof global.__godotQueueMicrotask === 'function'
        ? global.__godotQueueMicrotask(callback)
        : enqueueImmediate(callback),
  },
  NativeDOMCxx: global.__godotNativeDOM,
  NativeReactNativeFeatureFlagsCxx: {
    shouldPressibilityUseW3CPointerEventsForHover: () => true,
    enableImperativeFocus: () => true,
  },
  ExceptionsManager: {
    reportException: () => {},
    reportFatalException: () => {},
    reportSoftException: () => {},
    updateExceptionMessage: () => {},
    dismissRedbox: () => {},
  },
  Appearance: {
    getColorScheme: () => 'light',
    setColorScheme: () => unsupported('Appearance.setColorScheme'),
    addListener: () => {},
    removeListeners: () => {},
  },
  LogBox: {show: () => {}, hide: () => {}},
};

const nativeRegistry = global.__godotNativeModules;
global.__turboModuleProxy = name =>
  nativeRegistry?.get?.(name) ?? SHIMS[name] ?? null;
global.__godotUnsupported = unsupported;

function rejectionMessage(reason) {
  if (reason instanceof Error) {
    return String(reason.message || reason);
  }
  try {
    return typeof reason === 'string' ? reason : JSON.stringify(reason);
  } catch {
    return String(reason);
  }
}

global.__godotFinalizeBootstrap = development => {
  global.HermesInternal?.enablePromiseRejectionTracker?.({
    allRejections: true,
    onUnhandled(id, reason) {
      global.__godotReportRuntimeError?.({
        code: 'E_UNHANDLED_REJECTION',
        message: rejectionMessage(reason),
        operation: 'Promise.onUnhandled',
        rejectionId: id,
      });
    },
    onHandled(id) {
      global.__godotReportRuntimeError?.({
        code: 'E_UNHANDLED_REJECTION',
        message: development
          ? `Promise rejection handled (id: ${id})`
          : 'Promise rejection was handled after reporting.',
        operation: 'Promise.onHandled',
        rejectionId: id,
      });
    },
  });
};

if (!global.__DEV__) {
  global.__godotFinalizeBootstrap(false);
}
