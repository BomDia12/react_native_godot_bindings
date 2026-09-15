// React Native reads these globals during module initialization.

global.RN$Bridgeless = true;
global.RN$useAlwaysAvailableJSErrorHandling = true;

// NativeModules requires this object even in bridgeless mode.
global.__fbBatchedBridgeConfig = {remoteModuleConfig: []};

const CALLABLE_MODULES = {};
global.RN$registerCallableModule = (name, moduleOrFactory) => {
  CALLABLE_MODULES[name] = moduleOrFactory;
};
global.__godotCallableModules = CALLABLE_MODULES;

// ReactNativeRootView drains this host timer queue once per frame.
const TIMERS = new Map();
let nextTimerId = 1;

function schedule(fn, delayMs, repeatMs, args) {
  const id = nextTimerId++;
  TIMERS.set(id, {fn, args, due: Date.now() + (delayMs || 0), repeatMs});
  return id;
}

// Keep the original function because React Native replaces global.setImmediate.
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

  for (const [id, timer] of Array.from(TIMERS)) {
    if (timer.due > now) {
      continue;
    }

    if (timer.repeatMs == null) {
      TIMERS.delete(id);
    } else {
      timer.due = now + timer.repeatMs;
    }

    timer.fn(...timer.args);
  }
};

const MODULES = {
  PlatformConstants: {
    getConstants: () => ({
      isTesting: false,
      reactNativeVersion: {major: 0, minor: 87, patch: 1},
    }),
  },
  // AppContainer reads these values during startup.
  DeviceInfo: {
    getConstants: () => ({
      Dimensions: {
        window: {width: 800, height: 600, scale: 1, fontScale: 1},
        screen: {width: 800, height: 600, scale: 1, fontScale: 1},
      },
    }),
  },
  SourceCode: {getConstants: () => ({scriptURL: null})},
  NativeMicrotasksCxx: {queueMicrotask: callback => enqueueImmediate(callback)},
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
    setColorScheme: () => {},
    addListener: () => {},
    removeListeners: () => {},
  },
  LogBox: {show: () => {}, hide: () => {}},
};

global.__turboModuleProxy = name => MODULES[name] ?? null;
