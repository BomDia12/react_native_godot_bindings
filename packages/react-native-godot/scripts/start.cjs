#!/usr/bin/env node
'use strict';

// Serves the Godot project in the current directory the way `react-native
// start` serves an app: Metro with the project's Godot config, React Native's
// dev middleware for DevTools, and the /message socket that Metro's reload key
// and the runtime's packager connection share.
const path = require('path');
const {createRequire} = require('module');
const {parseArgs} = require('util');

const {values} = parseArgs({
  options: {
    port: {type: 'string', default: '8081'},
    host: {type: 'string', default: 'localhost'},
    config: {type: 'string', default: 'metro.config.godot.js'},
    'reset-cache': {type: 'boolean', default: false},
  },
});

const projectRoot = process.cwd();
const projectRequire = createRequire(path.join(projectRoot, 'package.json'));
const reactNativeRequire = createRequire(projectRequire.resolve('react-native/package.json'));
const Metro = projectRequire('metro');
const {Server: WebSocketServer} = createRequire(projectRequire.resolve('metro'))('ws');
const {createDevMiddleware} = reactNativeRequire('@react-native/dev-middleware');

function createMessageSocket() {
  const server = new WebSocketServer({noServer: true});
  const send = (message, sender) => {
    for (const client of server.clients) {
      if (client !== sender && client.readyState === client.OPEN) {
        client.send(message);
      }
    }
  };
  server.on('connection', client => client.on('message', data => send(data.toString(), client)));
  return {server, broadcast: method => send(JSON.stringify({version: 2, method}))};
}

function attachKeys(messages, serverBaseUrl) {
  if (!process.stdin.isTTY) {
    return;
  }
  process.stdin.setRawMode(true);
  process.stdin.setEncoding('utf8');
  process.stdin.on('data', key => {
    if (key === 'r') {
      messages.broadcast('reload');
    } else if (key === 'j') {
      fetch(serverBaseUrl + '/open-debugger', {method: 'POST'}).catch(() => {});
    } else if (key === '\u0003') {
      process.exit(0);
    }
  });
  console.info('Press r to reload the Godot runtimes, j to open DevTools, Ctrl+C to exit.');
}

async function main() {
  const port = Number(values.port);
  const serverBaseUrl = `http://${values.host}:${port}`;
  const config = await Metro.loadConfig({
    cwd: projectRoot,
    config: path.resolve(projectRoot, values.config),
    port,
    resetCache: values['reset-cache'],
  });
  const devTools = createDevMiddleware({serverBaseUrl});
  const messages = createMessageSocket();
  await Metro.runServer(config, {
    host: values.host,
    unstable_extraMiddleware: [devTools.middleware],
    websocketEndpoints: {...devTools.websocketEndpoints, '/message': messages.server},
    // Metro stops watching files when it detects a CI environment, which
    // would leave Fast Refresh without any edits to send.
    watch: true,
  });
  attachKeys(messages, serverBaseUrl);
}

main().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
