// Runs before any react-native module is evaluated (godot.entry.js imports it first),
// because RN reads these globals at module-init time.

// Bridgeless short-circuits most of RN's native surface before we have to implement it:
// the batched bridge (setUpBatchedBridge.js), the native Timing module (setUpTimers.js),
// UIManager (UIManager.js) and Text's hasViewManagerConfig round-trip
// (TextNativeComponent.js) all become no-ops or JS-only paths.
global.RN$Bridgeless = true;
global.RN$useAlwaysAvailableJSErrorHandling = true;

// TurboModuleRegistry imports the legacy BatchedBridge/NativeModules unconditionally,
// and that module asserts on __fbBatchedBridgeConfig at init even in bridgeless mode.
// An empty config makes NativeModules an empty object, which is exactly what we want:
// every lookup then falls through to __turboModuleProxy below.
global.__fbBatchedBridgeConfig = {remoteModuleConfig: []};

// In bridgeless mode the native runtime installs this; it is how JS modules make
// themselves callable from native (AppRegistry, RCTEventEmitter, ...). We keep the
// registry on the JS side so the native half can reach it once it needs to.
const CALLABLE_MODULES = {};
global.RN$registerCallableModule = (name, moduleOrFactory) => {
  CALLABLE_MODULES[name] = moduleOrFactory;
};
global.__godotCallableModules = CALLABLE_MODULES;

// Bare Hermes has no timers — they are part of the host, not the language — and
// bridgeless RN (setUpTimers.js) expects the runtime to have installed them already.
// This queue is drained once per frame by ReactNativeRootView, which calls
// __godotFlushTimers from _process.
const TIMERS = new Map();
let nextTimerId = 1;

function schedule(fn, delayMs, repeatMs, args) {
  const id = nextTimerId++;
  TIMERS.set(id, {fn, args, due: Date.now() + (delayMs || 0), repeatMs});
  return id;
}

// Kept as a direct handle on the queue: RN's setUpTimers replaces global.setImmediate
// with one that routes through the microtask module, so anything *implementing* that
// module must not call the global, or it recurses into itself.
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

// What is left of the native surface is small enough to fake in pure JS. No extra C++.
const MODULES = {
  PlatformConstants: {
    getConstants: () => ({
      isTesting: false,
      reactNativeVersion: {major: 0, minor: 84, patch: 1},
    }),
  },
  // Not optional: AppRegistry -> renderApplication wraps the app in AppContainer,
  // which reads Dimensions. Constants for now; feed it DisplayServer later.
  DeviceInfo: {
    getConstants: () => ({
      Dimensions: {
        window: {width: 800, height: 600, scale: 1, fontScale: 1},
        screen: {width: 800, height: 600, scale: 1, fontScale: 1},
      },
    }),
  },
  SourceCode: {getConstants: () => ({scriptURL: null})},
  // Hermes has no host microtask queue of its own; the frame queue is close enough
  // for a static render, and React only needs the callback to eventually run.
  NativeMicrotasksCxx: {queueMicrotask: callback => enqueueImmediate(callback)},
  // RN's DOM web-APIs (ReactNativeDocument, refs, getBoundingClientRect...). A static
  // render only needs linkRootNode to hand back *something* for the root; the rest is
  // reached through refs, which we do not support yet.
  NativeDOMCxx: {
    linkRootNode: rootTag => ({rootTag}),
    getParentNode: () => null,
    getChildNodes: () => [],
    isConnected: () => true,
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
