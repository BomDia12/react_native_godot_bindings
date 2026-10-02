'use strict';

const unsupported = operation => global.__godotUnsupported(operation);

export default {
  addListener: () => unsupported('RCTNetworking.addListener'),
  sendRequest: () => unsupported('RCTNetworking.sendRequest'),
  abortRequest: () => unsupported('RCTNetworking.abortRequest'),
  clearCookies: () => unsupported('RCTNetworking.clearCookies'),
};
