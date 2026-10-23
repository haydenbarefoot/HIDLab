
#pragma once

#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"hidlab(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>HIDLab</title>

  <style>
    * { box-sizing: border-box; }

    body {
      margin: 0;
      background: #111827;
      color: #f9fafb;
      font-family: system-ui, sans-serif;
    }

    main {
      max-width: 650px;
      margin: 32px auto;
      padding: 16px;
    }

    h1 { margin-bottom: 4px; }
    p, .muted { color: #9ca3af; }

    section {
      background: #1f2937;
      padding: 20px;
      border-radius: 12px;
      margin-top: 18px;
    }

    input, textarea {
      width: 100%;
      padding: 12px;
      margin: 6px 0;
      background: #111827;
      color: white;
      border: 1px solid #4b5563;
      border-radius: 8px;
      font: inherit;
    }

    textarea {
      min-height: 140px;
      resize: vertical;
    }

    button {
      border: 0;
      padding: 10px 12px;
      border-radius: 8px;
      background: #374151;
      color: white;
      font: inherit;
      cursor: pointer;
    }

    button:disabled {
      opacity: .4;
      cursor: default;
    }

    .primary { background: #2563eb; }
    .danger { background: #991b1b; }
    .full { width: 100%; margin-top: 8px; }

    .controls {
      display: grid;
      grid-template-columns: 1fr 1fr;
      gap: 10px;
    }

    .actions {
      display: flex;
      flex-wrap: wrap;
      gap: 8px;
      margin: 10px 0;
    }

    .item {
      padding: 12px 0;
      border-bottom: 1px solid #374151;
      overflow-wrap: anywhere;
    }

    .item:last-child { border-bottom: 0; }

    #queue, #networks {
      list-style: none;
      padding: 0;
    }

    #status { font-weight: bold; }

    [hidden] { display: none !important; }
  </style>
</head>

<body>
<main>
  <h1>HIDLab</h1>
  <p>USB HID Research Platform</p>

  <section>
    <h2>Device Status</h2>
    <div id="status">Connecting...</div>
    <p id="networkStatus"></p>
    <p id="sessionStatus" class="muted"></p>
    <p id="message" aria-live="polite"></p>
  </section>

  <section>
    <h2>Text Queue</h2>

    <textarea
      id="textInput"
      maxlength="12000"
      placeholder="Enter an ASCII passage..."
    ></textarea>

    <button class="primary full" onclick="action('add')">
      Add to Queue
    </button>

    <h3>Waiting Passages</h3>
    <ul id="queue"></ul>

    <div id="editor" hidden>
      <h3>Edit Passage</h3>
      <textarea id="editInput" maxlength="12000"></textarea>

      <div class="controls">
        <button class="primary" onclick="saveEdit()">Save</button>
        <button onclick="cancelEdit()">Cancel</button>
      </div>
    </div>
  </section>

  <section>
    <h2>Playback Controls</h2>

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
    <h2>Wi-Fi Networks</h2>
    <p>Saved network passwords are not displayed.</p>

    <ul id="networks"></ul>

    <h3>Add or Update Network</h3>

    <input id="ssid" maxlength="32"
           placeholder="Wi-Fi network name">

    <input id="password" type="password"
           maxlength="63"
           placeholder="Wi-Fi password">

    <button class="primary full" onclick="saveNetwork()">
      Save Network
    </button>

    <p class="muted">
      Saving a network may disconnect this webpage temporarily.
    </p>
  </section>
</main>

<script>
  const tokenKey = "hidlab-session-token";

  let token = localStorage.getItem(tokenKey);

  if (!token) {
    const bytes = new Uint8Array(24);
    crypto.getRandomValues(bytes);

    token = Array.from(bytes, byte =>
      byte.toString(16).padStart(2, "0")
    ).join("");

    localStorage.setItem(tokenKey, token);
  }

  let controller = false;
  let passages = [];
  let editingIndex = null;
  let busy = false;

  const element = id => document.getElementById(id);

  /**
   * Acquire or renew the browser's controller session.
   */
  async function claimSession() {
    try {
      const response = await fetch("/api/session", {
        method: "POST",
        headers: {"Content-Type": "application/json"},
        body: JSON.stringify({token})
      });

      controller = response.ok;

      element("sessionStatus").textContent = controller
        ? "Controller connected"
        : "Another device is controlling HIDLab";

    } catch {
      controller = false;
      element("sessionStatus").textContent =
        "Controller unavailable";
    }
  }

  /**
   * Submit an authenticated JSON command to an API endpoint.
   */
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

      element("message").textContent =
        result.message || result.error || "";

      if (response.status === 423) {
        controller = false;
      }

      return response.ok;

    } catch {
      element("message").textContent = "Connection lost.";
      return false;

    } finally {
      busy = false;
    }
  }

  /**
   * Execute a passage queue or playback command.
   */
  async function action(name, options = {}) {
    const success = await post("/api/action", {
      action: name,
      text: name === "add"
        ? element("textInput").value
        : (options.text ?? ""),
      index: options.index ?? null,
      direction: options.direction ?? null
    });

    if (success) {
      if (name === "add") {
        element("textInput").value = "";
      }

      if (["remove", "move", "start", "stop"].includes(name)) {
        cancelEdit();
      }
    }

    await refresh();
  }

  /** Open the passage editor without changing queued text. */
  function beginEdit(index) {
    editingIndex = index;
    element("editInput").value = passages[index];
    element("editor").hidden = false;
    element("editInput").focus();
  }

  /** Save the passage currently being edited. */
  async function saveEdit() {
    if (editingIndex === null) return;

    const success = await post("/api/action", {
      action: "edit",
      index: editingIndex,
      text: element("editInput").value
    });

    if (success) {
      cancelEdit();
    }

    await refresh();
  }

  /** Dismiss the editor without modifying the queue. */
  function cancelEdit() {
    editingIndex = null;
    element("editInput").value = "";
    element("editor").hidden = true;
  }

  /** Build an action button for a queue or network item. */
  function makeButton(label, handler, disabled = false, css = "") {
    const button = document.createElement("button");
    button.textContent = label;
    button.type = "button";
    button.disabled = disabled || !controller;
    button.className = css;
    button.addEventListener("click", handler);
    return button;
  }

  /** Render the waiting queue and its management controls. */
  function renderQueue() {
    const list = element("queue");
    list.replaceChildren();

    passages.forEach((text, index) => {
      const item = document.createElement("li");
      item.className = "item";

      const preview = document.createElement("div");
      preview.textContent =
        `Passage ${index + 1}: ${text.slice(0, 100)}` +
        (text.length > 100 ? "..." : "");

      const buttons = document.createElement("div");
      buttons.className = "actions";

      buttons.append(
        makeButton("Edit", () => beginEdit(index)),
        makeButton(
          "Up",
          () => action("move", {index, direction: "up"}),
          index === 0
        ),
        makeButton(
          "Down",
          () => action("move", {index, direction: "down"}),
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
      list.appendChild(item);
    });

    if (editingIndex !== null &&
        editingIndex >= passages.length) {
      cancelEdit();
    }
  }

  /** Display saved network names and removal controls. */
  async function refreshNetworks() {
    try {
      const response = await fetch("/api/networks");
      const data = await response.json();

      const list = element("networks");
      list.replaceChildren();

      data.networks.forEach((ssid, index) => {
        const item = document.createElement("li");
        item.className = "item";

        const label = document.createElement("span");
        label.textContent = ssid + " ";

        const button = makeButton(
          "Remove",
          () => removeNetwork(index),
          false,
          "danger"
        );

        item.append(label, button);
        list.appendChild(item);
      });
    } catch {
      element("message").textContent =
        "Could not load saved networks.";
    }
  }

  /** Save a Wi-Fi profile on the device. */
  async function saveNetwork() {
    const success = await post("/api/networks/action", {
      action: "add",
      ssid: element("ssid").value,
      password: element("password").value
    });

    if (success) {
      element("ssid").value = "";
      element("password").value = "";
    }

    await refreshNetworks();
  }

  /** Delete a Wi-Fi profile by its displayed index. */
  async function removeNetwork(index) {
    await post("/api/networks/action", {
      action: "remove",
      index
    });

    await refreshNetworks();
  }

  /** Refresh playback, queue, and connectivity indicators. */
  async function refresh() {
    try {
      const response = await fetch("/api/status", {
        cache: "no-store"
      });

      if (!response.ok) {
        throw new Error("Status unavailable");
      }

      const data = await response.json();

      element("status").textContent = data.status.toUpperCase();

      element("networkStatus").textContent =
        data.setup_mode
          ? `Setup network: ${data.ip}`
          : `Wi-Fi: ${data.network || "Connecting"} (${data.ip})`;

      passages = data.passages;
      renderQueue();

      element("start").disabled =
        !controller || data.status !== "idle" || data.queued === 0;

      element("pause").disabled =
        !controller || data.status !== "typing";

      element("resume").disabled =
        !controller || data.status !== "paused";

      element("stop").disabled =
        !controller || data.status === "idle";

    } catch {
      element("status").textContent = "DISCONNECTED";

      for (const id of ["start", "pause", "resume", "stop"]) {
        element(id).disabled = true;
      }
    }
  }

  /** Initialize and periodically synchronize the interface. */
  async function initialize() {
    await claimSession();
    await refresh();
    await refreshNetworks();

    setInterval(refresh, 1000);
    setInterval(claimSession, 10000);
    setInterval(refreshNetworks, 10000);
  }

  initialize();
</script>
</body>
</html>
)hidlab";
