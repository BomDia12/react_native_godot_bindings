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

const scheduler = global.__godotScheduler;
if (scheduler == null) {
  throw new Error('Godot native scheduler must be installed before bootstrap.');
}
for (const name of [
  'setTimeout', 'setInterval', 'clearTimeout', 'clearInterval',
  'requestAnimationFrame', 'cancelAnimationFrame',
]) {
  global[name] = scheduler[name];
}
global.__blobCollectorProvider = global.__godotBlobCollectors?.create;
global.nativePerformanceNow = scheduler.now;
global.performance = {now: scheduler.now};

const unsupported = operation => {
  const error = new Error(operation + ' is pending on the Godot platform.');
  error.code = 'E_UNSUPPORTED';
  error.operation = operation;
  throw error;
};

const SHIMS = {
  SourceCode: {getConstants: () => ({scriptURL: null})},
  NativeMicrotasksCxx: {
    queueMicrotask: callback =>
      typeof global.__godotQueueMicrotask === 'function'
        ? global.__godotQueueMicrotask(callback)
        : unsupported('queueMicrotask'),
  },
  NativeIdleCallbacksCxx: scheduler,
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
  LogBox: {show: () => {}, hide: () => {}},
};

const nativeRegistry = global.__godotNativeModules;
global.__turboModuleProxy = name =>
  global.__godotNativeFacade?.(name) ?? nativeRegistry?.get?.(name) ?? SHIMS[name] ?? null;
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
