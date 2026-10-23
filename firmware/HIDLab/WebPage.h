
#pragma once

#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"HIDLAB(
<!doctype html>
<html lang="en">

<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>HIDLab</title>

<style>
  * {
    box-sizing: border-box;
  }

  body {
    background: #111827;
    color: #f9fafb;
    font: 16px system-ui, sans-serif;
    margin: 0;
  }

  main {
    max-width: 720px;
    margin: 24px auto;
    padding: 14px;
  }

  h1 { margin: 0; }

  h2 {
    font-size: 1.2rem;
    margin-top: 0;
  }

  p, .muted {
    color: #9ca3af;
  }

  section {
    background: #1f2937;
    border-radius: 12px;
    padding: 19px;
    margin-top: 18px;
  }

  input, textarea {
    display: block;
    width: 100%;
    background: #111827;
    color: #fff;
    border: 1px solid #64748b;
    border-radius: 8px;
    padding: 12px;
    font: inherit;
    margin: 8px 0;
  }

  textarea {
    min-height: 140px;
    resize: vertical;
  }

  button {
    background: #374151;
    color: #fff;
    border: 0;
    border-radius: 8px;
    padding: 10px 12px;
    font: inherit;
    cursor: pointer;
  }

  button:disabled {
    opacity: .35;
    cursor: not-allowed;
  }

  .primary { background: #2563eb; }
  .danger { background: #991b1b; }
  .full { width: 100%; }

  .controls {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 10px;
  }

  .actions {
    display: flex;
    gap: 8px;
    flex-wrap: wrap;
    margin-top: 9px;
  }

  .item {
    padding: 12px 0;
    border-bottom: 1px solid #374151;
    overflow-wrap: anywhere;
  }

  .item:last-child {
    border-bottom: none;
  }

  ul {
    padding: 0;
    list-style: none;
  }

  #status {
    font-weight: 700;
  }

  [hidden] {
    display: none !important;
  }

  .small {
    font-size: .85rem;
  }

  #message {
    min-height: 1.5em;
  }

  @media (max-width: 440px) {
    main { padding: 10px; }
    section { padding: 15px; }
    .actions button { font-size: .86rem; }
  }
</style>
</head>

<body>
<main>

<header>
  <h1>HIDLab</h1>
  <p>USB HID research platform</p>
</header>

<section>
  <h2>Device</h2>

  <div id="status">Connecting...</div>
  <p class="small" id="usb"></p>
  <p class="small" id="network"></p>

  <p id="controller" class="muted">
    Connecting controller...
  </p>

  <button id="reclaim" onclick="requestControl()" hidden>
    Request control
  </button>

  <button id="release" onclick="releaseControl()" hidden>
    Release control
  </button>

  <p id="message" role="status"></p>
</section>

<section>
  <h2>Queue</h2>

  <textarea
    id="newText"
    maxlength="12000"
    placeholder="Enter plain ASCII text..."
  ></textarea>

  <button
    id="add"
    class="primary full"
    onclick="action('add')"
  >
    Add to queue
  </button>

  <ul id="queue"></ul>
  <p id="noQueue" class="muted">No passages waiting.</p>

  <div id="editor" hidden>
    <h2>Edit passage</h2>

    <textarea id="editText" maxlength="12000"></textarea>

    <div class="controls">
      <button class="primary" onclick="saveEdit()">
        Save changes
      </button>

      <button onclick="cancelEdit()">
        Cancel
      </button>
    </div>
  </div>
</section>

<section>
  <h2>Playback</h2>

  <div class="controls">
    <button id="start" class="primary"
      onclick="action('start')">Start</button>

    <button id="pause"
      onclick="action('pause')">Pause</button>

    <button id="resume"
      onclick="action('resume')">Resume</button>

    <button id="stop"
      onclick="action('stop')">Stop</button>
  </div>
</section>

<section>
  <h2>Saved Wi-Fi networks</h2>

  <ul id="networks"></ul>

  <input
    id="ssid"
    maxlength="32"
    placeholder="Network name (SSID)"
    autocomplete="off"
  >

  <input
    id="password"
    type="password"
    maxlength="63"
    placeholder="Network password"
    autocomplete="off"
  >

  <button
    id="saveNetwork"
    class="primary full"
    onclick="saveNetwork()"
  >
    Save network
  </button>

  <p class="small">
    Saving or removing a network can change the connection.
    Passwords are never displayed.
  </p>
</section>

</main>

<script>
const $ = id => document.getElementById(id);

const key = 'hidlab-controller-token';
let token = localStorage.getItem(key);

if (!token) {
  const bytes = new Uint8Array(24);
  crypto.getRandomValues(bytes);

  token = Array.from(
    bytes,
    x => x.toString(16).padStart(2, '0')
  ).join('');

  localStorage.setItem(key, token);
}

let controller = false;
let autoClaim = true;
let busy = false;
let passages = [];
let editing = null;
let lastStatus = null;


/** Acquire or renew the single-controller session. */
async function claimSession() {
  if (!autoClaim) return;

  try {
    const response = await fetch('/api/session', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json'
      },
      body: JSON.stringify({ token })
    });

    controller = response.ok;

    $('controller').textContent = controller
      ? 'Controller connected'
      : 'Another controller is connected';

  } catch {
    controller = false;
    $('controller').textContent = 'Controller disconnected';
  }

  $('release').hidden = !controller;
  $('reclaim').hidden = controller;

  setControls();
}


async function requestControl() {
  autoClaim = true;

  await claimSession();
  await refresh();
  await refreshNetworks();
}


async function releaseControl() {
  if (!controller) return;

  try {
    await fetch('/api/release', {
      method: 'POST',
      headers: {
        'X-HIDLab-Session': token
      }
    });
  } catch {}

  controller = false;
  autoClaim = false;

  $('controller').textContent = 'Controller released';
  $('release').hidden = true;
  $('reclaim').hidden = false;

  setControls();
}


/** Send an authenticated JSON command to the device. */
async function post(path, payload) {
  if (!controller || busy) return false;

  busy = true;

  try {
    const response = await fetch(path, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'X-HIDLab-Session': token
      },
      body: JSON.stringify(payload)
    });

    const data = await response.json();

    $('message').textContent =
      data.message || data.error || '';

    if (response.status === 423) {
      controller = false;
      await claimSession();
    }

    return response.ok;

  } catch {
    $('message').textContent = 'Connection lost';
    return false;

  } finally {
    busy = false;
  }
}


/** Submit a queue or playback operation. */
async function action(command, options = {}) {
  const text = command === 'add'
    ? $('newText').value
    : (options.text ?? '');

  if (
    (command === 'add' || command === 'edit') &&
    (/[^\x0A\x0D\x20-\x7E]/.test(text))
  ) {
    $('message').textContent =
      'Only basic ASCII characters and line breaks are supported.';
    return;
  }

  const ok = await post('/api/action', {
    action: command,
    text,
    index: options.index ?? null,
    direction: options.direction ?? null
  });

  if (ok) {
    if (command === 'add') {
      $('newText').value = '';
    }

    if (['remove', 'move', 'start', 'stop'].includes(command)) {
      cancelEdit();
    }
  }

  await refresh();
}


function beginEdit(index) {
  if (!controller || index < 0 || index >= passages.length) {
    return;
  }

  editing = index;
  $('editText').value = passages[index];
  $('editor').hidden = false;
  $('editText').focus();
}


function cancelEdit() {
  editing = null;
  $('editor').hidden = true;
  $('editText').value = '';
}


async function saveEdit() {
  if (editing === null) return;

  const index = editing;
  const text = $('editText').value;

  if (/[^\x0A\x0D\x20-\x7E]/.test(text)) {
    $('message').textContent =
      'Only basic ASCII characters and line breaks are supported.';
    return;
  }

  const ok = await post('/api/action', {
    action: 'edit',
    index,
    text
  });

  if (ok) cancelEdit();

  await refresh();
}


/** Create queue and network controls using escaped text. */
function button(label, callback, disabled = false, css = '') {
  const b = document.createElement('button');

  b.type = 'button';
  b.textContent = label;
  b.className = css;
  b.disabled = disabled || !controller;
  b.addEventListener('click', callback);

  return b;
}


function renderQueue() {
  const list = $('queue');
  list.replaceChildren();

  $('noQueue').hidden = passages.length > 0;

  passages.forEach((text, index) => {
    const li = document.createElement('li');
    li.className = 'item';

    const preview = document.createElement('div');

    preview.textContent =
      `Passage ${index + 1}: ${text.slice(0, 110)}` +
      (text.length > 110 ? '...' : '');

    const controls = document.createElement('div');
    controls.className = 'actions';

    controls.append(
      button('Edit', () => beginEdit(index)),

      button(
        'Up',
        () => action('move', { index, direction: 'up' }),
        index === 0
      ),

      button(
        'Down',
        () => action('move', { index, direction: 'down' }),
        index === passages.length - 1
      ),

      button(
        'Remove',
        () => action('remove', { index }),
        false,
        'danger'
      )
    );

    li.append(preview, controls);
    list.append(li);
  });

  if (editing !== null && editing >= passages.length) {
    cancelEdit();
  }
}


function setControls() {
  const s = lastStatus || {};

  $('add').disabled = !controller || busy;
  $('saveNetwork').disabled = !controller || busy;

  $('start').disabled =
    !controller ||
    s.status !== 'idle' ||
    !s.queued ||
    !s.usb_ready;

  $('pause').disabled =
    !controller || s.status !== 'typing';

  $('resume').disabled =
    !controller ||
    s.status !== 'paused' ||
    !s.usb_ready;

  $('stop').disabled =
    !controller || s.status === 'idle';
}


/** Poll device status and the waiting queue. */
async function refresh() {
  if (!controller) {
    $('status').textContent = 'LOCKED';
    setControls();
    return;
  }

  try {
    const response = await fetch('/api/status', {
      headers: {
        'X-HIDLab-Session': token
      },
      cache: 'no-store'
    });

    if (!response.ok) {
      throw Error('Status unavailable');
    }

    const data = await response.json();
    lastStatus = data;

    $('status').textContent = data.status.toUpperCase();

    $('usb').textContent = data.usb_ready
      ? 'USB keyboard connected'
      : 'USB keyboard unavailable';

    $('network').textContent = data.setup_mode
      ? `Setup Wi-Fi: ${data.ip}`
      : `Wi-Fi: ${data.network || 'Offline'} ` +
        `(${data.ip || 'no address'})`;

    passages = data.passages;

    renderQueue();
    setControls();

  } catch {
    $('status').textContent = 'DISCONNECTED';
    lastStatus = null;
    setControls();
  }
}


async function refreshNetworks() {
  if (!controller) return;

  try {
    const response = await fetch('/api/networks', {
      headers: {
        'X-HIDLab-Session': token
      }
    });

    if (!response.ok) {
      throw Error('Networks unavailable');
    }

    const data = await response.json();
    const list = $('networks');

    list.replaceChildren();

    data.networks.forEach((ssid, index) => {
      const li = document.createElement('li');
      li.className = 'item';

      const label = document.createElement('span');
      label.textContent = ssid + ' ';

      li.append(
        label,
        button(
          'Remove',
          () => removeNetwork(index),
          false,
          'danger'
        )
      );

      list.append(li);
    });

  } catch {
    $('message').textContent = 'Cannot load network list';
  }
}


async function saveNetwork() {
  const ok = await post('/api/networks/action', {
    action: 'add',
    ssid: $('ssid').value,
    password: $('password').value
  });

  if (ok) {
    $('ssid').value = '';
    $('password').value = '';
  }

  await refreshNetworks();
}


async function removeNetwork(index) {
  await post('/api/networks/action', {
    action: 'remove',
    index
  });

  await refreshNetworks();
}


/** Initialize and periodically synchronize the interface. */
async function initialize() {
  await claimSession();
  await refresh();
  await refreshNetworks();

  setInterval(refresh, 1500);
  setInterval(claimSession, 8000);
  setInterval(refreshNetworks, 12000);
}

initialize();
</script>
</body>
</html>
)HIDLAB";
