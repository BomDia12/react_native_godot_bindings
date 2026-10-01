'use strict';

const unsupported = () => global.__godotUnsupported('Android-only API');
export default new Proxy(
  {},
  {
    get: () => unsupported,
  },
);
