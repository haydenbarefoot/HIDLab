
#pragma once

#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"HIDLAB(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0b1220">
<title>HIDLab</title>
<style>
:root{color-scheme:dark;--bg:#0b1220;--card:#172236;--field:#0e1829;--border:#324258;--text:#f3f7ff;--muted:#a1b3c9;--accent:#73b0ff;--blue:#276fe4;--red:#b94051}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:16px -apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;-webkit-text-size-adjust:100%}
main{width:min(100%,700px);margin:0 auto;padding:18px 14px max(30px,env(safe-area-inset-bottom))}
header{display:flex;justify-content:space-between;align-items:center;gap:14px;padding:12px 2px 6px}
h1{font-size:1.7rem;letter-spacing:-.03em;margin:0}
.kicker{font-size:.7rem;color:var(--accent);letter-spacing:.14em;font-weight:800;text-transform:uppercase}
.sub{color:var(--muted);font-size:.89rem;margin:5px 0 0}
.pill{font-size:.75rem;font-weight:700;border:1px solid #3c5471;border-radius:999px;padding:8px 11px;color:#b8d7ff;white-space:nowrap}
.card{background:var(--card);border:1px solid #283b56;border-radius:16px;padding:19px;margin-top:15px;box-shadow:0 8px 32px #00000016}
.card h2{font-size:1.05rem;letter-spacing:-.01em;margin:0 0 14px}
.muted{color:var(--muted)}.small{font-size:.83rem}.helper{font-size:.81rem;line-height:1.45;color:var(--muted);margin:9px 0 0}
.statusLine{display:flex;align-items:center;gap:9px;font-weight:800;letter-spacing:.02em}
.dot{width:9px;height:9px;border-radius:50%;background:#8796ad;display:inline-block}
.dot.on{background:#42d6a4}.dot.off{background:#ffbe69}
#details{font-size:.85rem;color:var(--muted);margin:10px 0 0;line-height:1.6;overflow-wrap:anywhere}
#controller{color:var(--muted);font-size:.86rem;margin:9px 0 0}
button,input,textarea{font:inherit}
button{min-height:46px;touch-action:manipulation;border-radius:10px;border:1px solid var(--border);background:#293b54;color:var(--text);padding:10px 14px;cursor:pointer;font-weight:650}
button:active:not(:disabled){transform:translateY(1px)}button:disabled{opacity:.38;cursor:default}
button.primary{background:var(--blue);border-color:#397ef1}button.danger{background:#592634;border-color:#884050}
button.ghost{background:transparent}
.row{display:flex;align-items:center;gap:9px;flex-wrap:wrap}.row>*{flex:1}
.controls{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.full{width:100%}
textarea,input[type=text],input[type=password]{display:block;width:100%;border:1px solid #41516a;border-radius:10px;background:var(--field);color:var(--text);padding:12px;font-size:16px;outline:none}
textarea:focus,input:focus-visible{border-color:var(--accent)}
textarea{min-height:136px;resize:vertical;line-height:1.5}
label.heading{display:flex;justify-content:space-between;align-items:baseline;gap:12px;font-weight:650;font-size:.92rem}
output{font-variant-numeric:tabular-nums;color:#c4dcff;font-weight:750}
.rangeBox{padding:13px 0 6px}
input[type=range]{-webkit-appearance:none;appearance:none;width:100%;height:7px;border-radius:999px;background:#475975;outline:none;margin:22px 0 12px;touch-action:pan-y}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;appearance:none;width:30px;height:30px;border-radius:50%;background:var(--accent);border:4px solid #20344f;box-shadow:0 1px 5px #0006}
input[type=range]::-moz-range-thumb{width:23px;height:23px;border-radius:50%;background:var(--accent);border:4px solid #20344f}
.ends{display:flex;justify-content:space-between;color:var(--muted);font-size:.74rem}
.segment{display:grid;grid-template-columns:1fr 1fr;gap:6px;padding:5px;background:#0e1829;border-radius:12px}
.segment button{border:0;background:transparent;min-height:47px}.segment button.selected{background:#314e78;color:#fff;box-shadow:0 1px 4px #0004}
#humanOptions[hidden], [hidden]{display:none!important}
.list{list-style:none;padding:0;margin:12px 0 0}.item{padding:13px 0;border-top:1px solid #34445b;overflow-wrap:anywhere}
.item .preview{font-size:.9rem;line-height:1.5;white-space:pre-wrap;overflow-wrap:anywhere;max-height:5em;overflow:hidden}
.item .actions{display:flex;flex-wrap:wrap;gap:7px;margin-top:10px}
.item .actions button{min-height:42px;flex:1;font-size:.86rem;padding:8px 10px}
input[type=text],input[type=password]{margin-top:9px}
#message{font-size:.88rem;min-height:1em;margin:9px 0 0;color:#c5ddff;overflow-wrap:anywhere}
.caption{font-size:.76rem;color:var(--muted);line-height:1.5}
@media(min-width:600px){main{padding:27px}.card{padding:23px}}
@media(max-width:370px){main{padding:12px 10px}.card{padding:15px}.pill{font-size:.68rem;padding:7px}}
</style>
</head>
<body>
<main>
<header>
  <div><div class="kicker">USB keyboard lab</div><h1>HIDLab</h1><p class="sub">Control from your iPhone or browser</p></div>
  <div class="pill" id="chip">CONNECTING</div>
</header>

<section class="card" aria-label="Device status">
  <div class="statusLine"><span class="dot" id="statusDot"></span><span id="status">Connecting...</span></div>
  <div id="details">Checking device connection.</div>
  <p id="controller">Connecting controller...</p>
  <div class="row" style="margin-top:12px">
    <button class="ghost" id="reclaim" onclick="requestControl()" hidden>Request control</button>
    <button class="ghost" id="release" onclick="releaseControl()" hidden>Release control</button>
  </div>
  <div id="message" role="status" aria-live="polite"></div>
</section>

<section class="card">
  <h2>Typing mode</h2>
  <div class="segment" role="group" aria-label="Typing mode">
    <button id="modeStandard" onclick="selectMode('standard')">Standard</button>
    <button id="modeHumanized" onclick="selectMode('humanized')">Humanized</button>
  </div>
  <div id="humanOptions">
    <div class="rangeBox">
      <label class="heading" for="wpm">Average speed <output id="wpmValue">35 WPM</output></label>
      <input id="wpm" type="range" min="15" max="100" step="5" value="35" oninput="updateRangeLabels()">
      <div class="ends"><span>15 WPM</span><span>100 WPM</span></div>
    </div>
    <div class="rangeBox">
      <label class="heading" for="typos">Correction frequency <output id="typosValue">3%</output></label>
      <input id="typos" type="range" min="0" max="10" step="1" value="3" oninput="updateRangeLabels()">
      <div class="ends"><span>No intentional typos</span><span>10% of letters</span></div>
    </div>
    <p class="helper">Varied timing, occasional nearby-key mistakes, and Backspace corrections. Final text remains unchanged when delivered successfully.</p>
  </div>
  <button class="primary full" id="saveSettings" onclick="saveSettings()" style="margin-top:15px">Save typing settings</button>
  <p class="caption">Settings can be changed when playback is idle. They persist after restarting the device.</p>
</section>

<section class="card">
  <h2>Passage queue</h2>
  <textarea id="newText" maxlength="12000" placeholder="Type or paste ASCII text..."></textarea>
  <button class="primary full" id="add" onclick="action('add')" style="margin-top:10px">Add passage</button>
  <ul class="list" id="queue"></ul><p id="noQueue" class="helper">No passages waiting.</p>
  <div id="editor" hidden>
    <h2 style="margin-top:14px">Edit passage</h2>
    <textarea id="editText" maxlength="12000"></textarea>
    <div class="controls" style="margin-top:9px">
      <button class="primary" onclick="saveEdit()">Save changes</button>
      <button onclick="cancelEdit()">Cancel</button>
    </div>
  </div>
</section>

<section class="card">
  <h2>Playback</h2>
  <div class="controls">
    <button class="primary" id="start" onclick="action('start')">Start</button>
    <button id="pause" onclick="action('pause')">Pause</button>
    <button id="resume" onclick="action('resume')">Resume</button>
    <button class="danger" id="stop" onclick="action('stop')">Stop</button>
  </div>
  <p class="caption">Make sure the target game's text field is selected before pressing Start. Output requires an active USB connection.</p>
</section>

<section class="card">
  <h2>Saved Wi-Fi networks</h2>
  <ul class="list" id="networks"></ul>
  <input id="ssid" type="text" maxlength="32" placeholder="Network name (SSID)" autocomplete="off">
  <input id="password" type="password" maxlength="63" placeholder="Network password" autocomplete="off">
  <button class="primary full" id="saveNetwork" onclick="saveNetwork()" style="margin-top:12px">Save network</button>
  <p class="caption">Changing networks may temporarily disconnect this page. Passwords are never displayed.</p>
</section>
</main>
<script>
const $ = id => document.getElementById(id);
const tokenKey = 'hidlab-controller-token';
let token = localStorage.getItem(tokenKey);
if (!token) {
  const bytes = new Uint8Array(24);
  crypto.getRandomValues(bytes);
  token = Array.from(bytes, v => v.toString(16).padStart(2, '0')).join('');
  localStorage.setItem(tokenKey, token);
}
let controller = false;
let autoClaim = true;
let busy = false;
let passages = [];
let editing = null;
let selectedMode = 'standard';
let lastStatus = null;
let lastQueueKey = '';

function updateRangeLabels() {
  $('wpmValue').textContent = $('wpm').value + ' WPM';
  $('typosValue').textContent = $('typos').value + '%';
}
function selectMode(mode) {
  selectedMode = mode;
  $('modeStandard').classList.toggle('selected', mode === 'standard');
  $('modeHumanized').classList.toggle('selected', mode === 'humanized');
  $('modeStandard').setAttribute('aria-pressed', mode === 'standard');
  $('modeHumanized').setAttribute('aria-pressed', mode === 'humanized');
  $('humanOptions').hidden = mode !== 'humanized';
}
async function claimSession() {
  if (!autoClaim) return;
  try {
    const response = await fetch('/api/session', {
      method: 'POST', headers: {'Content-Type':'application/json'},
      body: JSON.stringify({token})
    });
    controller = response.ok;
    $('controller').textContent = controller ? 'Controller connected' : 'Another browser has control';
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
  await refreshSettings();
  await refreshNetworks();
}
async function releaseControl() {
  if (!controller) return;
  try {
    await fetch('/api/release', {method:'POST',headers:{'X-HIDLab-Session':token}});
  } catch {}
  controller = false;
  autoClaim = false;
  $('controller').textContent = 'Controller released';
  $('release').hidden = true;
  $('reclaim').hidden = false;
  setControls();
}
async function post(path, payload) {
  if (!controller || busy) return false;
  busy = true;
  try {
    const response = await fetch(path, {
      method:'POST',
      headers:{'Content-Type':'application/json','X-HIDLab-Session':token},
      body: JSON.stringify(payload)
    });
    const data = await response.json();
    $('message').textContent = data.message || data.error || '';
    if (response.status === 423) controller = false;
    return response.ok;
  } catch {
    $('message').textContent = 'Connection lost';
    return false;
  } finally {
    busy = false;
    setControls();
  }
}
async function getApi(path) {
  const response = await fetch(path, {
    headers:{'X-HIDLab-Session':token},cache:'no-store'
  });
  if (!response.ok) throw Error('API unavailable');
  return response.json();
}
async function refreshSettings() {
  if (!controller) return;
  try {
    const settings = await getApi('/api/settings');
    selectMode(settings.mode);
    $('wpm').value = settings.wpm;
    $('typos').value = settings.typo_rate;
    updateRangeLabels();
  } catch { $('message').textContent = 'Could not load typing settings'; }
}
async function saveSettings() {
  const ok = await post('/api/settings', {
    mode:selectedMode,wpm:Number($('wpm').value),typo_rate:Number($('typos').value)
  });
  if (ok) await refreshSettings();
}
async function action(command, options = {}) {
  const text = command === 'add' ? $('newText').value : (options.text ?? '');
  if (command === 'add' && /[^\x0A\x0D\x20-\x7E]/.test(text)) {
    $('message').textContent = 'Only ASCII text and line breaks are supported.';
    return;
  }
  const ok = await post('/api/action', {
    action:command,text,index:options.index ?? null,direction:options.direction ?? null
  });
  if (ok) {
    if (command === 'add') $('newText').value = '';
    if (['remove','move','start','stop'].includes(command)) cancelEdit();
  }
  await refresh();
}
function beginEdit(index) {
  if (!controller || index < 0 || index >= passages.length) return;
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
  const text = $('editText').value;
  if (/[^\x0A\x0D\x20-\x7E]/.test(text)) {
    $('message').textContent = 'Only ASCII text and line breaks are supported.';
    return;
  }
  const ok = await post('/api/action', {action:'edit',index:editing,text});
  if (ok) cancelEdit();
  await refresh();
}
function button(label, handler, disabled = false, css = '') {
  const b = document.createElement('button');
  b.type = 'button';b.textContent = label;b.className = css;
  b.disabled = disabled || !controller;
  b.addEventListener('click',handler);
  return b;
}
function renderQueue() {
  const list = $('queue');
  list.replaceChildren();
  $('noQueue').hidden = passages.length > 0;
  passages.forEach((text,index) => {
    const item = document.createElement('li');item.className = 'item';
    const preview = document.createElement('div');preview.className = 'preview';
    preview.textContent = 'Passage ' + (index+1) + ': ' + text.slice(0,110) + (text.length>110?'...':'');
    const buttons = document.createElement('div');buttons.className = 'actions';
    buttons.append(
      button('Edit',()=>beginEdit(index)),
      button('Up',()=>action('move',{index,direction:'up'}),index===0),
      button('Down',()=>action('move',{index,direction:'down'}),index===passages.length-1),
      button('Remove',()=>action('remove',{index}),false,'danger')
    );
    item.append(preview,buttons);list.append(item);
  });
  if (editing !== null && editing >= passages.length) cancelEdit();
}
function setControls() {
  const s = lastStatus || {};
  $('add').disabled = !controller || busy;
  $('saveNetwork').disabled = !controller || busy;
  const changing = !controller || busy || s.status !== 'idle';
  $('saveSettings').disabled = changing;
  $('wpm').disabled = changing;
  $('typos').disabled = changing;
  $('modeStandard').disabled = changing;
  $('modeHumanized').disabled = changing;
  $('start').disabled = !controller || s.status !== 'idle' || !s.queued || !s.usb_ready;
  $('pause').disabled = !controller || s.status !== 'typing';
  $('resume').disabled = !controller || s.status !== 'paused' || !s.usb_ready;
  $('stop').disabled = !controller || s.status === 'idle';
}
async function refresh() {
  if (!controller) {
    $('status').textContent = 'LOCKED';
    $('chip').textContent = 'LOCKED';
    $('statusDot').className = 'dot off';
    lastStatus = null;
    setControls();return;
  }
  try {
    const data = await getApi('/api/status');
    lastStatus = data;
    $('status').textContent = data.status.toUpperCase();
    $('chip').textContent = data.status.toUpperCase();
    $('statusDot').className = data.usb_ready ? 'dot on' : 'dot off';
    $('details').textContent = (data.usb_ready?'USB keyboard ready':'USB unavailable') +
      ' · ' + (data.setup_mode?'Setup Wi-Fi':(data.network || 'Wi-Fi connecting')) +
      (data.ip?' · '+data.ip:'') + (data.usb_paused?' · Resume required':'');
    const key = JSON.stringify(data.passages) + ':' + controller;
    if (key !== lastQueueKey) {
      passages = data.passages;
      lastQueueKey = key;
      renderQueue();
    }
    setControls();
  } catch {
    $('status').textContent = 'DISCONNECTED';
    $('chip').textContent = 'OFFLINE';
    $('statusDot').className = 'dot off';
    lastStatus = null;setControls();
  }
}
async function refreshNetworks() {
  if (!controller) return;
  try {
    const data = await getApi('/api/networks');
    const list = $('networks');list.replaceChildren();
    data.networks.forEach((ssid,index) => {
      const li = document.createElement('li');li.className = 'item';
      const label = document.createElement('span');label.textContent = ssid + ' ';
      li.append(label,button('Remove',()=>removeNetwork(index),false,'danger'));
      list.append(li);
    });
  } catch { $('message').textContent = 'Cannot load network list'; }
}
async function saveNetwork() {
  const ok = await post('/api/networks/action',{
    action:'add',ssid:$('ssid').value,password:$('password').value
  });
  if (ok) {$('ssid').value='';$('password').value='';}
  await refreshNetworks();
}
async function removeNetwork(index) {
  await post('/api/networks/action',{action:'remove',index});
  await refreshNetworks();
}
async function initialize() {
  selectMode('standard');updateRangeLabels();
  await claimSession();await refresh();await refreshSettings();await refreshNetworks();
  setInterval(refresh,1500);
  setInterval(claimSession,8000);
  setInterval(refreshNetworks,12000);
}
initialize();
</script>
</body>
</html>
)HIDLAB";
