import {Image} from 'react-native';
import {binaryService} from '../../../js/godot/binary';

export async function runHTTPChecks(base, https) {
  const evidence = [];
  const check = (condition, name) => {if (!condition) {throw new Error(name);} evidence.push(name);};
  const fetchText = async (path, options) => (await fetch(base + path, options)).text();
  const binary = await fetchText('/binary');
  check(binary === 'A\0B\ufffd', 'binary UTF-8 preserves NUL and replacement');
  const failureStatus = await fetch(base + '/status/404');
  check(failureStatus.status === 404 && !failureStatus.ok, 'HTTP status remains a response');
  check(await fetchText('/compressed') === 'compressed\0payload', 'engine gzip decompression');
  const view = new Uint8Array([9, 65, 0, 66, 9]);
  const echoed = await fetchText('/echo', {method: 'POST', body: view.subarray(1, 4)});
  check(echoed === 'A\0B', 'ArrayBufferView byte offsets');
  const blob = new Blob(['a\0b', new Blob(['cd'])]);
  const slice = blob.slice(1, 4);
  const reader = new FileReader();
  const read = value => new Promise((resolve, reject) => {
    reader.onload = () => resolve(reader.result); reader.onerror = reject; reader.readAsText(value);
  });
  check(await read(slice) === '\0bc', 'Blob parts and slices');
  const form = new FormData();
  form.append('label', 'hello');
  form.append('file', {uri: 'res://assets/item.png', name: 'item.png', type: 'image/png'});
  form.append('body', blob);
  const multipart = await fetchText('/echo', {method: 'POST', body: form});
  check(multipart.includes('name="label"') && multipart.includes('filename="item.png"') && multipart.includes('a\0bcd'), 'native multipart strings local files and Blob');
  const http = global.__godotNativeModules.get('GodotHTTP');
  http.clearCookies();
  await fetch(base + '/set-cookies', {credentials: 'include'});
  check((await fetchText('/cookies', {credentials: 'include'})).includes('a=one'), 'repeated Set-Cookie headers');
  check(await fetchText('/cookies', {credentials: 'omit'}) === '', 'credential omission');
  const redirected = await fetch(base + '/redirect', {credentials: 'include'});
  check(redirected.url === base + '/cookies' && (await redirected.text()).includes('redirect=seen'), 'redirect final URL and cookie hop');
  const queryRedirect = await fetch(base + '/nested/query-redirect?old=1');
  check(queryRedirect.url === base + '/nested/query-redirect?page=2' && await queryRedirect.text() === 'query redirect passed', 'query-only redirect preserves current path');
  check(await fetchText('/relative-redirect') === 'A\0B\ufffd', 'root relative redirect has one leading slash');
  await new Promise((resolve, reject) => {
    const xhr = new XMLHttpRequest();
    const states = [];
    xhr.onreadystatechange = () => states.push(xhr.readyState);
    xhr.open('GET', base + '/binary');
    xhr.onload = () => { try {check(xhr.responseText === 'A\0B\ufffd' && states.includes(2) && states.includes(3) && states.at(-1) === 4, 'complete XHR response data completion ordering'); resolve();} catch (error) {reject(error);} };
    xhr.onerror = reject; xhr.send();
  });
  await new Promise((resolve, reject) => {
    const xhr = new XMLHttpRequest(); xhr.open('GET', base + '/slow?seconds=1'); xhr.timeout = 50;
    xhr.ontimeout = () => {evidence.push('native timeout'); resolve();}; xhr.onload = () => reject(new Error('timeout did not expire')); xhr.onerror = reject; xhr.send();
  });
  await new Promise((resolve, reject) => {
    const xhr = new XMLHttpRequest(); xhr.open('GET', base + '/slow?seconds=1'); xhr.onabort = () => {evidence.push('queued abort'); resolve();}; xhr.onload = () => reject(new Error('aborted request loaded')); xhr.send(); xhr.abort();
  });
  const canceled = new FileReader();
  const cancelBlob = new Blob(['cancel'.repeat(300000)]);
  let abortCount = 0;
  canceled.onabort = () => {abortCount++;};
  canceled.readAsText(cancelBlob); canceled.abort(); cancelBlob.close();
  await new Promise(resolve => setTimeout(resolve, 0));
  check(abortCount === 1 && canceled.result == null, 'FileReader abort releases native pin');
  const objectBlob = new Blob(['URL\0bytes']);
  const objectURL = URL.createObjectURL(objectBlob);
  objectBlob.close();
  check(await (await fetch(objectURL)).text() === 'URL\0bytes', 'object URL pins closed Blob');
  URL.revokeObjectURL(objectURL);
  let revoked = false;
  try {await fetch(objectURL);} catch (error) {revoked = true;}
  check(revoked, 'revoked object URL rejects fetch');
  const parallel = await Promise.all(Array.from({length: 12}, () => fetchText('/binary')));
  check(parallel.every(value => value === 'A\0B\ufffd'), 'bounded pool queued reuse');
  const source = {uri: base + '/icon.png', credentials: 'omit'};
  const size = await Image.getSize(source);
  check(size.width === 24 && size.height === 24, 'shared native image size transport');
  check(await Image.prefetch(source), 'shared image prefetch');
  const cached = await Image.queryCache([source]);
  check(cached[source.uri] === 'memory', 'stateless image cache');
  const privateSize = await Image.getSizeWithHeaders(source.uri, {Authorization: 'Bearer fixture'});
  check(privateSize.width === 24, 'credentialed image request is isolated');
  let prefetchId;
  const canceledPrefetch = Image.prefetch({uri: base + '/slow?seconds=1', credentials: 'omit'}, id => {prefetchId = id;}).then(() => false, error => error.code === 'E_CANCELLED');
  Image.abortPrefetch(prefetchId);
  check(await canceledPrefetch, 'image prefetch cancellation releases its subscriber');
  http.setTrustResource('res://fixtures/tls/ca.pem');
  check(await (await fetch(https + '/binary')).text() === 'A\0B\ufffd', 'verified fixture TLS');
  blob.close(); slice.close();
  const large = new Blob(['x'.repeat(17 * 1024 * 1024 + 7)]);
  let limited = false;
  try {binaryService().begin('over-budget', 17 * 1024 * 1024);} catch (error) {limited = error.code === 'E_LIMIT';}
  check(limited, 'aggregate Blob configuration ceiling');
  check((await read(large)).length === 17 * 1024 * 1024 + 7, 'Blob reads exceed 16 MiB without codec expansion');
  large.close();
  for (let i = 0; i < 24; i++) {await fetchText('/binary');}
  check(binaryService().stats().bytes < 1024, 'repeated text fetch native reclamation');
  const stats = http.stats();
  check(stats.active + stats.idle + stats.draining <= 8 && stats.active <= 8, 'configured pool node bound');
  return evidence;
}

export async function runSocketChecks(url, applyUpdate) {
  const extra = [];
  await new Promise((resolve, reject) => {
    const opening = new WebSocket(url, ['fixture']);
    let closes = 0;
    opening.onclose = () => {closes++; setTimeout(() => {if (closes === 1) {extra.push('close before open once'); resolve();} else {reject(new Error('duplicate early close'));}}, 0);};
    opening.onerror = () => {};
    opening.close(1000, 'early');
  });
  await new Promise((resolve, reject) => {
    const invalid = new WebSocket(url, ['fixture']);
    invalid.onopen = () => {
      let failures = 0;
      try {invalid.close(2000);} catch (error) {failures++;}
      try {invalid.close(1000, 'x'.repeat(124));} catch (error) {failures++;}
      if (failures !== 2) {reject(new Error('invalid close accepted')); return;}
      extra.push('invalid close code and UTF-8 reason'); invalid.close(1000, 'validation');
    };
    invalid.onclose = resolve; invalid.onerror = reject;
  });

  await new Promise((resolve, reject) => {
    const flood = new WebSocket(url, ['fixture']);
    let messages = 0;
    flood.onopen = () => {
      try {flood.send('x'.repeat(1024 * 1024 + 1)); reject(new Error('oversized send accepted'));}
      catch (error) {if (error.code !== 'E_LIMIT') {reject(error); return;}}
      flood.send('flood');
    };
    flood.onerror = reject;
    flood.onmessage = event => {
      if (event.data !== String(messages++)) {reject(new Error('flood ordering')); return;}
      if (messages === 300) {flood.close(1000, 'drained');}
      else if (messages % 32 === 0) {flood.send('ack');}
    };
    flood.onclose = () => {if (messages === 300) {extra.push('bounded flood backpressure and message limit'); resolve();} else {reject(new Error('flood dropped messages'));}};
  });
  await new Promise((resolve, reject) => {
    const broken = new WebSocket(url, ['fixture']);
    let closes = 0;
    broken.onopen = () => broken.send('unclean');
    broken.onerror = () => {};
    broken.onclose = event => {if (++closes === 1 && event.code === 1006 && !event.wasClean) {extra.push('unclean close'); resolve();} else {reject(new Error('invalid unclean close'));}};
  });
  return new Promise((resolve, reject) => {
    const socket = new WebSocket(url, ['fixture']);
    socket.binaryType = 'arraybuffer';
    const evidence = [...extra];
    let messages = 0;
    socket.onopen = () => {
      if (socket.protocol !== 'fixture') {reject(new Error('subprotocol was not negotiated')); return;}
      evidence.push('native subprotocol'); socket.send('echo');
    };
    socket.onerror = () => reject(new Error('socket failed'));
    socket.onmessage = async event => {
      try {
        messages++;
        if (messages === 1) {
          if (event.data !== 'echo') {throw new Error('native text echo');}
          evidence.push('native text'); socket.send(new Uint8Array([9, 65, 0, 66, 9]).subarray(1, 4));
        } else if (messages === 2) {
          const bytes = new Uint8Array(event.data);
          if (bytes.length !== 3 || bytes[0] !== 65 || bytes[1] !== 0 || bytes[2] !== 66) {throw new Error('native binary echo');}
          evidence.push('native binary offsets NUL'); socket.send('updates');
        } else {
          await applyUpdate(JSON.parse(event.data)); evidence.push('network enters Godot authority'); socket.close(1000, 'done');
        }
      } catch (error) {socket.close(); reject(error);}
    };
    socket.onclose = event => {
      if (messages !== 3 || event.code !== 1000 || !event.wasClean) {reject(new Error('clean close did not complete: ' + JSON.stringify({messages, code: event.code, clean: event.wasClean}))); return;}
      evidence.push('clean close'); resolve(evidence);
    };
  });
}
