
#pragma once

#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"HIDLAB(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport"
      content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0b1220">
<title>HIDLab</title>

<style>
:root {
  color-scheme: dark;
  --bg: #0b1220;
  --card: #172236;
  --field: #0e1829;
  --border: #324258;
  --text: #f3f7ff;
  --muted: #a1b3c9;
  --accent: #73b0ff;
  --blue: #276fe4;
  --red: #b94051;
}

* {
  box-sizing: border-box;
}

body {
  margin: 0;
  background: var(--bg);
  color: var(--text);
  font: 16px -apple-system, BlinkMacSystemFont,
        "Segoe UI", sans-serif;
  -webkit-text-size-adjust: 100%;
}

main {
  width: min(100%, 700px);
  margin: 0 auto;
  padding: 18px 14px max(30px, env(safe-area-inset-bottom));
}

header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 14px;
  padding: 12px 2px 6px;
}

h1 {
  font-size: 1.7rem;
  letter-spacing: -0.03em;
  margin: 0;
}

h2 {
  font-size: 1.05rem;
  margin: 0 0 14px;
}

.kicker {
  font-size: 0.7rem;
  color: var(--accent);
  letter-spacing: 0.14em;
  font-weight: 800;
  text-transform: uppercase;
}

.sub {
  color: var(--muted);
  font-size: 0.89rem;
  margin: 5px 0 0;
}

.pill {
  font-size: 0.75rem;
  font-weight: 700;
  border: 1px solid #3c5471;
  border-radius: 999px;
  padding: 8px 11px;
  color: #b8d7ff;
  white-space: nowrap;
}

.card {
  background: var(--card);
  border: 1px solid #283b56;
  border-radius: 16px;
  padding: 19px;
  margin-top: 15px;
}

.muted,
.helper,
.caption {
  color: var(--muted);
}

.helper,
.caption {
  font-size: 0.81rem;
  line-height: 1.5;
}

.status-line {
  display: flex;
  align-items: center;
  gap: 9px;
  font-weight: 800;
}

.dot {
  width: 9px;
  height: 9px;
  border-radius: 50%;
  background: #8796ad;
}

.dot.on {
  background: #42d6a4;
}

.dot.off {
  background: #ffbe69;
}

#details {
  font-size: 0.85rem;
  color: var(--muted);
  margin: 10px 0;
  line-height: 1.6;
  overflow-wrap: anywhere;
}

#controller,
#message {
  font-size: 0.86rem;
  overflow-wrap: anywhere;
}

#controller {
  color: var(--muted);
}

#message {
  min-height: 1.3em;
  color: #c5ddff;
}

button,
input,
textarea {
  font: inherit;
}

button {
  min-height: 46px;
  touch-action: manipulation;
  border-radius: 10px;
  border: 1px solid var(--border);
  background: #293b54;
  color: var(--text);
  padding: 10px 14px;
  cursor: pointer;
  font-weight: 650;
}

button:disabled {
  opacity: 0.38;
  cursor: default;
}

button.primary {
  background: var(--blue);
  border-color: #397ef1;
}

button.danger {
  background: #592634;
  border-color: #884050;
}

button.ghost {
  background: transparent;
}

button:active:not(:disabled) {
  transform: translateY(1px);
}

.full {
  width: 100%;
}

.controls {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 10px;
}

.row {
  display: flex;
  align-items: center;
  gap: 9px;
  flex-wrap: wrap;
}

.segment {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 6px;
  padding: 5px;
  background: var(--field);
  border-radius: 12px;
}

.segment button {
  border: 0;
  background: transparent;
  min-height: 47px;
}

.segment button.selected {
  background: #314e78;
  color: white;
}

.range-box {
  padding: 16px 0 6px;
}

.heading {
  display: flex;
  justify-content: space-between;
  align-items: baseline;
  gap: 12px;
  font-weight: 650;
  font-size: 0.92rem;
}

output {
  color: #c4dcff;
  font-weight: 750;
  font-variant-numeric: tabular-nums;
  white-space: nowrap;
}

input[type="range"] {
  -webkit-appearance: none;
  appearance: none;
  width: 100%;
  height: 7px;
  border-radius: 999px;
  background: #475975;
  outline: none;
  margin: 23px 0 14px;
  touch-action: pan-y;
}

input[type="range"]::-webkit-slider-thumb {
  -webkit-appearance: none;
  appearance: none;
  width: 30px;
  height: 30px;
  border-radius: 50%;
  background: var(--accent);
  border: 4px solid #20344f;
  box-shadow: 0 1px 5px #0006;
}

input[type="range"]::-moz-range-thumb {
  width: 23px;
  height: 23px;
  border-radius: 50%;
  background: var(--accent);
  border: 4px solid #20344f;
}

.ends {
  display: flex;
  justify-content: space-between;
  color: var(--muted);
  font-size: 0.75rem;
}

.range-summary {
  margin-top: 15px;
  background: var(--field);
  border: 1px solid #34445b;
  border-radius: 12px;
  padding: 14px;
}

.range-summary strong {
  display: block;
  margin-top: 4px;
  font-size: 1.35rem;
  color: #c4dcff;
  font-variant-numeric: tabular-nums;
}

textarea,
input[type="text"],
input[type="password"] {
  display: block;
  width: 100%;
  border: 1px solid #41516a;
  border-radius: 10px;
  background: var(--field);
  color: var(--text);
  padding: 12px;
  font-size: 16px;
  outline: none;
}

textarea {
  min-height: 136px;
  resize: vertical;
  line-height: 1.5;
}

input[type="text"],
input[type="password"] {
  margin-top: 9px;
}

textarea:focus,
input:focus-visible {
  border-color: var(--accent);
}

.list {
  list-style: none;
  padding: 0;
  margin: 12px 0 0;
}

.item {
  padding: 13px 0;
  border-top: 1px solid #34445b;
  overflow-wrap: anywhere;
}

.preview {
  font-size: 0.9rem;
  line-height: 1.5;
  white-space: pre-wrap;
  overflow-wrap: anywhere;
  max-height: 5em;
  overflow: hidden;
}

.actions {
  display: flex;
  flex-wrap: wrap;
  gap: 7px;
  margin-top: 10px;
}

.actions button {
  min-height: 42px;
  flex: 1;
  font-size: 0.86rem;
  padding: 8px 10px;
}

[hidden] {
  display: none !important;
}

@media (min-width: 600px) {
  main {
    padding: 27px;
  }

  .card {
    padding: 23px;
  }
}

@media (max-width: 370px) {
  main {
    padding: 12px 10px;
  }

  .card {
    padding: 15px;
  }

  .pill {
    font-size: 0.68rem;
    padding: 7px;
  }
}
</style>
</head>

<body>
<main>

<header>
  <div>
    <div class="kicker">USB keyboard lab</div>
    <h1>HIDLab</h1>
    <p class="sub">Wireless keyboard control</p>
  </div>

  <div class="pill" id="chip">CONNECTING</div>
</header>

<section class="card">
  <div class="status-line">
    <span class="dot" id="statusDot"></span>
    <span id="status">Connecting...</span>
  </div>

  <div id="details">Checking device connection.</div>

  <p id="controller">Connecting controller...</p>

  <div class="row">
    <button
      class="ghost"
      id="reclaim"
      onclick="requestControl()"
      hidden
    >
      Request control
    </button>

    <button
      class="ghost"
      id="release"
      onclick="releaseControl()"
      hidden
    >
      Release control
    </button>
  </div>

  <p id="message" role="status" aria-live="polite"></p>
</section>

<section class="card">
  <h2>Typing Mode</h2>

  <div class="segment" role="group" aria-label="Typing mode">
    <button
      id="modeStandard"
      onclick="selectMode('standard')"
    >
      Standard
    </button>

    <button
      id="modeHumanized"
      onclick="selectMode('humanized')"
    >
      Humanized
    </button>
  </div>

  <div id="humanOptions" hidden>

    <div class="range-box">
      <label class="heading" for="minWpm">
        Minimum speed
        <output id="minWpmValue">30 WPM</output>
      </label>

      <input
        id="minWpm"
        type="range"
        min="15"
        max="100"
        step="1"
        value="30"
        oninput="updateWpmRange('min')"
      >

      <div class="ends">
        <span>15 WPM</span>
        <span>100 WPM</span>
      </div>
    </div>

    <div class="range-box">
      <label class="heading" for="maxWpm">
        Maximum speed
        <output id="maxWpmValue">35 WPM</output>
      </label>

      <input
        id="maxWpm"
        type="range"
        min="15"
        max="100"
        step="1"
        value="35"
        oninput="updateWpmRange('max')"
      >

      <div class="ends">
        <span>15 WPM</span>
        <span>100 WPM</span>
      </div>
    </div>

    <div class="range-summary">
      <div class="helper">Selected typing range</div>
      <strong id="rangeValue">30–35 WPM</strong>
    </div>

    <div class="range-box">
      <label class="heading" for="typos">
        Correction frequency
        <output id="typosValue">3%</output>
      </label>

      <input
        id="typos"
        type="range"
        min="0"
        max="10"
        step="1"
        value="3"
        oninput="updateRangeLabels()"
      >

      <div class="ends">
        <span>No added typos</span>
        <span>10% of letters</span>
      </div>
    </div>

    <p class="helper">
      The target speed changes gradually as text is typed.
      Occasional nearby-key mistakes are corrected with Backspace.
    </p>
  </div>

  <button
    class="primary full"
    id="saveSettings"
    onclick="saveSettings()"
    style="margin-top:15px"
  >
    Save typing settings
  </button>

  <p class="caption">
    Settings are saved on the device and can be changed while
    playback is idle.
  </p>
</section>

<section class="card">
  <h2>Passage Queue</h2>

  <textarea
    id="newText"
    maxlength="12000"
    placeholder="Type or paste ASCII text..."
  ></textarea>

  <button
    class="primary full"
    id="add"
    onclick="action('add')"
    style="margin-top:10px"
  >
    Add passage
  </button>

  <ul class="list" id="queue"></ul>

  <p id="noQueue" class="helper">
    No passages waiting.
  </p>

  <div id="editor" hidden>
    <h2 style="margin-top:14px">Edit passage</h2>

    <textarea
      id="editText"
      maxlength="12000"
    ></textarea>

    <div class="controls" style="margin-top:9px">
      <button class="primary" onclick="saveEdit()">
        Save changes
      </button>

      <button onclick="cancelEdit()">
        Cancel
      </button>
    </div>
  </div>
</section>

<section class="card">
  <h2>Playback</h2>

  <div class="controls">
    <button
      class="primary"
      id="start"
      onclick="action('start')"
    >
      Start
    </button>

    <button
      id="pause"
      onclick="action('pause')"
    >
      Pause
    </button>

    <button
      id="resume"
      onclick="action('resume')"
    >
      Resume
    </button>

    <button
      class="danger"
      id="stop"
      onclick="action('stop')"
    >
      Stop
    </button>
  </div>

  <p class="caption">
    Select the destination text field before pressing Start.
    USB output requires a connected keyboard host.
  </p>
</section>

<section class="card">
  <h2>Saved Wi-Fi Networks</h2>

  <ul class="list" id="networks"></ul>

  <input
    id="ssid"
    type="text"
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
    class="primary full"
    id="saveNetwork"
    onclick="saveNetwork()"
    style="margin-top:12px"
  >
    Save network
  </button>

  <p class="caption">
    Changing networks can temporarily disconnect the page.
    Saved passwords are not displayed.
  </p>
</section>

</main>

<script>
const $ = id => document.getElementById(id);

const tokenKey = "hidlab-controller-token";
let token;

try {
  token = localStorage.getItem(tokenKey);
} catch {
  token = null;
}

if (!token) {
  const bytes = new Uint8Array(24);
  crypto.getRandomValues(bytes);

  token = Array.from(
    bytes,
    value => value.toString(16).padStart(2, "0")
  ).join("");

  try {
    localStorage.setItem(tokenKey, token);
  } catch {}
}

let controller = false;
let autoClaim = true;
let busy = false;
let passages = [];
let editing = null;
let selectedMode = "standard";
let lastStatus = null;
let lastQueueKey = "";


/** Keep the two WPM sliders in a valid minimum/maximum range. */
function updateWpmRange(changed) {
  const minSlider = $("minWpm");
  const maxSlider = $("maxWpm");

  let minimum = Number(minSlider.value);
  let maximum = Number(maxSlider.value);

  if (minimum > maximum) {
    if (changed === "min") {
      maximum = minimum;
      maxSlider.value = maximum;
    } else {
      minimum = maximum;
      minSlider.value = minimum;
    }
  }

  updateRangeLabels();
}


/** Display the current speed and typo settings. */
function updateRangeLabels() {
  const minimum = Number($("minWpm").value);
  const maximum = Number($("maxWpm").value);

  $("minWpmValue").textContent = minimum + " WPM";
  $("maxWpmValue").textContent = maximum + " WPM";
  $("rangeValue").textContent =
    minimum + "–" + maximum + " WPM";

  $("typosValue").textContent =
    $("typos").value + "%";
}


/** Switch between Standard and Humanized input. */
function selectMode(mode) {
  selectedMode = mode;

  $("modeStandard").classList.toggle(
    "selected",
    mode === "standard"
  );

  $("modeHumanized").classList.toggle(
    "selected",
    mode === "humanized"
  );

  $("modeStandard").setAttribute(
    "aria-pressed",
    mode === "standard"
  );

  $("modeHumanized").setAttribute(
    "aria-pressed",
    mode === "humanized"
  );

  $("humanOptions").hidden =
    mode !== "humanized";
}


/** Acquire or renew the current controller session. */
async function claimSession() {
  if (!autoClaim) {
    return;
  }

  try {
    const response = await fetch("/api/session", {
      method: "POST",
      headers: {
        "Content-Type": "application/json"
      },
      body: JSON.stringify({token})
    });

    controller = response.ok;

    $("controller").textContent = controller
      ? "Controller connected"
      : "Another browser has control";

  } catch {
    controller = false;
    $("controller").textContent =
      "Controller disconnected";
  }

  $("release").hidden = !controller;
  $("reclaim").hidden = controller;

  setControls();
}


/** Request control after releasing or losing a session. */
async function requestControl() {
  autoClaim = true;

  await claimSession();
  await refresh();
  await refreshSettings();
  await refreshNetworks();
}


/** Release control without stopping a passage. */
async function releaseControl() {
  if (!controller) {
    return;
  }

  try {
    await fetch("/api/release", {
      method: "POST",
      headers: {
        "X-HIDLab-Session": token
      }
    });
  } catch {}

  controller = false;
  autoClaim = false;

  $("controller").textContent =
    "Controller released";

  $("release").hidden = true;
  $("reclaim").hidden = false;

  setControls();
}


/** Send an authenticated API command. */
async function post(path, payload) {
  if (!controller || busy) {
    return false;
  }

  busy = true;

  try {
    const response = await fetch(path, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
        "X-HIDLab-Session": token
      },
      body: JSON.stringify(payload)
    });

    const result = await response.json();

    $("message").textContent =
      result.message || result.error || "";

    if (response.status === 423) {
      controller = false;
    }

    return response.ok;

  } catch {
    $("message").textContent =
      "Connection lost.";
    return false;

  } finally {
    busy = false;
    setControls();
  }
}


/** Get an API response using the current controller token. */
async function getApi(path) {
  const response = await fetch(path, {
    headers: {
      "X-HIDLab-Session": token
    },
    cache: "no-store"
  });

  if (!response.ok) {
    throw new Error("API unavailable");
  }

  return response.json();
}


/** Load saved typing settings into the controls. */
async function refreshSettings() {
  if (!controller) {
    return;
  }

  try {
    const settings = await getApi("/api/settings");

    selectMode(settings.mode);

    $("minWpm").value = settings.min_wpm;
    $("maxWpm").value = settings.max_wpm;
    $("typos").value = settings.typo_rate;

    updateRangeLabels();

  } catch {
    $("message").textContent =
      "Could not load typing settings.";
  }
}


/** Persist the two WPM limits and correction frequency. */
async function saveSettings() {
  const success = await post("/api/settings", {
    mode: selectedMode,
    min_wpm: Number($("minWpm").value),
    max_wpm: Number($("maxWpm").value),
    typo_rate: Number($("typos").value)
  });

  if (success) {
    await refreshSettings();
  }
}


/** Submit a passage or playback action. */
async function action(command, options = {}) {
  const text = command === "add"
    ? $("newText").value
    : (options.text ?? "");

  if (command === "add" &&
      /[^\x0A\x0D\x20-\x7E]/.test(text)) {

    $("message").textContent =
      "Only basic ASCII and line breaks are supported.";
    return;
  }

  const success = await post("/api/action", {
    action: command,
    text,
    index: options.index ?? null,
    direction: options.direction ?? null
  });

  if (success) {
    if (command === "add") {
      $("newText").value = "";
    }

    if (["remove", "move", "start", "stop"].includes(command)) {
      cancelEdit();
    }
  }

  await refresh();
}


/** Open the editor for one waiting passage. */
function beginEdit(index) {
  if (!controller ||
      index < 0 ||
      index >= passages.length) {
    return;
  }

  editing = index;

  $("editText").value =
    passages[index];

  $("editor").hidden = false;
  $("editText").focus();
}


/** Dismiss the passage editor. */
function cancelEdit() {
  editing = null;
  $("editor").hidden = true;
  $("editText").value = "";
}


/** Save edited passage text. */
async function saveEdit() {
  if (editing === null) {
    return;
  }

  const text = $("editText").value;

  if (/[^\x0A\x0D\x20-\x7E]/.test(text)) {
    $("message").textContent =
      "Only basic ASCII and line breaks are supported.";
    return;
  }

  const success = await post("/api/action", {
    action: "edit",
    index: editing,
    text
  });

  if (success) {
    cancelEdit();
  }

  await refresh();
}


/** Build a touch-friendly action button. */
function makeButton(
  label,
  handler,
  disabled = false,
  css = ""
) {
  const element = document.createElement("button");

  element.type = "button";
  element.textContent = label;
  element.className = css;
  element.disabled = disabled || !controller;

  element.addEventListener("click", handler);

  return element;
}


/** Display waiting passages and queue controls. */
function renderQueue() {
  const list = $("queue");

  list.replaceChildren();

  $("noQueue").hidden =
    passages.length > 0;

  passages.forEach((text, index) => {
    const item = document.createElement("li");
    item.className = "item";

    const preview = document.createElement("div");
    preview.className = "preview";

    preview.textContent =
      "Passage " + (index + 1) + ": " +
      text.slice(0, 110) +
      (text.length > 110 ? "..." : "");

    const buttons = document.createElement("div");
    buttons.className = "actions";

    buttons.append(
      makeButton(
        "Edit",
        () => beginEdit(index)
      ),

      makeButton(
        "Up",
        () => action("move", {
          index,
          direction: "up"
        }),
        index === 0
      ),

      makeButton(
        "Down",
        () => action("move", {
          index,
          direction: "down"
        }),
        index === passages.length - 1
      ),

      makeButton(
        "Remove",
        () => action("remove", {index}),
        false,
        "danger"
      )
    );

    item.append(preview, buttons);
    list.append(item);
  });

  if (editing !== null &&
      editing >= passages.length) {
    cancelEdit();
  }
}


/** Enable only controls that are valid for the current state. */
function setControls() {
  const status = lastStatus || {};

  $("add").disabled = !controller || busy;
  $("saveNetwork").disabled = !controller || busy;

  const settingsLocked =
    !controller ||
    busy ||
    status.status !== "idle";

  $("saveSettings").disabled = settingsLocked;
  $("modeStandard").disabled = settingsLocked;
  $("modeHumanized").disabled = settingsLocked;
  $("minWpm").disabled = settingsLocked;
  $("maxWpm").disabled = settingsLocked;
  $("typos").disabled = settingsLocked;

  $("start").disabled =
    !controller ||
    status.status !== "idle" ||
    !status.queued ||
    !status.usb_ready;

  $("pause").disabled =
    !controller ||
    status.status !== "typing";

  $("resume").disabled =
    !controller ||
    status.status !== "paused" ||
    !status.usb_ready;

  $("stop").disabled =
    !controller ||
    status.status === "idle";
}


/** Poll playback and connection status. */
async function refresh() {
  if (!controller) {
    $("status").textContent = "LOCKED";
    $("chip").textContent = "LOCKED";
    $("statusDot").className = "dot off";

    lastStatus = null;
    setControls();
    return;
  }

  try {
    const status = await getApi("/api/status");

    lastStatus = status;

    $("status").textContent =
      status.status.toUpperCase();

    $("chip").textContent =
      status.status.toUpperCase();

    $("statusDot").className =
      status.usb_ready ? "dot on" : "dot off";

    let details = status.usb_ready
      ? "USB keyboard ready"
      : "USB unavailable";

    details += " · ";

    details += status.setup_mode
      ? "Setup Wi-Fi"
      : (status.network || "Wi-Fi connecting");

    if (status.ip) {
      details += " · " + status.ip;
    }

    if (status.usb_paused) {
      details += " · Resume required";
    }

    if (status.status === "typing") {
      details += " · Target " +
        status.current_wpm + " WPM";
    }

    $("details").textContent = details;

    const queueKey =
      JSON.stringify(status.passages) +
      ":" + controller;

    if (queueKey !== lastQueueKey) {
      passages = status.passages;
      lastQueueKey = queueKey;
      renderQueue();
    }

    setControls();

  } catch {
    $("status").textContent = "DISCONNECTED";
    $("chip").textContent = "OFFLINE";
    $("statusDot").className = "dot off";

    lastStatus = null;
    setControls();
  }
}


/** Show saved Wi-Fi names without revealing passwords. */
async function refreshNetworks() {
  if (!controller) {
    return;
  }

  try {
    const data = await getApi("/api/networks");
    const list = $("networks");

    list.replaceChildren();

    data.networks.forEach((ssid, index) => {
      const item = document.createElement("li");
      item.className = "item";

      const name = document.createElement("span");
      name.textContent = ssid + " ";

      const removeButton = makeButton(
        "Remove",
        () => removeNetwork(index),
        false,
        "danger"
      );

      item.append(name, removeButton);
      list.append(item);
    });

  } catch {
    $("message").textContent =
      "Cannot load saved networks.";
  }
}


/** Save a Wi-Fi network on the device. */
async function saveNetwork() {
  const success = await post("/api/networks/action", {
    action: "add",
    ssid: $("ssid").value,
    password: $("password").value
  });

  if (success) {
    $("ssid").value = "";
    $("password").value = "";
  }

  await refreshNetworks();
}


/** Remove a saved Wi-Fi network. */
async function removeNetwork(index) {
  await post("/api/networks/action", {
    action: "remove",
    index
  });

  await refreshNetworks();
}


/** Start the interface and background status polling. */
async function initialize() {
  selectMode("standard");
  updateRangeLabels();

  await claimSession();
  await refresh();
  await refreshSettings();
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
