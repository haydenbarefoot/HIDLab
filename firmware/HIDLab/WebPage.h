
#pragma once

#include <Arduino.h>

// Browser interface stored in firmware flash memory.
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
      margin: 30px auto;
      padding: 16px;
    }

    h1 { margin-bottom: 4px; }

    p { color: #9ca3af; }

    .panel {
      background: #1f2937;
      padding: 20px;
      border-radius: 12px;
      margin-top: 20px;
    }

    textarea {
      width: 100%;
      min-height: 150px;
      padding: 12px;
      background: #111827;
      color: white;
      border: 1px solid #4b5563;
      border-radius: 8px;
      font: inherit;
      resize: vertical;
    }

    button {
      padding: 10px 12px;
      background: #374151;
      border: 0;
      border-radius: 8px;
      color: white;
      font: inherit;
      cursor: pointer;
    }

    button:disabled {
      opacity: 0.4;
      cursor: default;
    }

    .primary { background: #2563eb; }
    .danger { background: #991b1b; }

    .full-width {
      width: 100%;
      margin-top: 12px;
    }

    .controls {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 10px;
    }

    #status { font-weight: bold; }
    #message { min-height: 20px; }

    #queue {
      list-style: none;
      padding: 0;
    }

    .queue-item {
      padding: 12px 0;
      border-bottom: 1px solid #374151;
    }

    .preview {
      overflow-wrap: anywhere;
      margin-bottom: 10px;
    }

    .actions {
      display: flex;
      flex-wrap: wrap;
      gap: 8px;
    }

    .actions button {
      font-size: 14px;
      padding: 8px 10px;
    }

    #editPanel {
      background: #111827;
      border: 1px solid #4b5563;
      border-radius: 8px;
      padding: 14px;
      margin-top: 16px;
    }

    [hidden] { display: none !important; }
  </style>
</head>

<body>
  <main>
    <h1>HIDLab</h1>
    <p>Arduino Nano ESP32 — Local Control Interface</p>

    <section class="panel">
      <h2>Device Status</h2>
      <div id="status">Connecting...</div>
      <p id="message" aria-live="polite"></p>
    </section>

    <section class="panel">
      <h2>Text Queue</h2>

      <textarea
        id="textInput"
        maxlength="12000"
        placeholder="Enter a passage..."
        aria-label="New passage"
      ></textarea>

      <button id="add" class="primary full-width"
              onclick="sendAction('add')">
        Add to Queue
      </button>

      <h3>Queued Passages</h3>
      <ul id="queue"></ul>
      <p id="emptyQueue">No passages waiting.</p>

      <div id="editPanel" hidden>
        <h3>Edit Passage</h3>

        <textarea id="editInput"
                  maxlength="12000"
                  aria-label="Edit passage"></textarea>

        <div class="controls">
          <button class="primary" onclick="saveEdit()">
            Save
          </button>
          <button onclick="cancelEdit()">
            Cancel
          </button>
        </div>
      </div>
    </section>

    <section class="panel">
      <h2>Playback Controls</h2>

      <div class="controls">
        <button id="start" class="primary"
                onclick="sendAction('start')">Start</button>
        <button id="pause"
                onclick="sendAction('pause')">Pause</button>
        <button id="resume"
                onclick="sendAction('resume')">Resume</button>
        <button id="stop"
                onclick="sendAction('stop')">Stop</button>
      </div>
    </section>
  </main>

  <script>
    let passages = [];
    let editingIndex = null;
    let requestPending = false;

    /**
     * Send a queue or playback command to the device.
     * @returns {Promise<boolean>} Whether the request succeeded.
     */
    async function sendAction(action, options = {}) {
      if (requestPending) return false;

      requestPending = true;

      const input = document.getElementById("textInput");
      const message = document.getElementById("message");

      try {
        const response = await fetch("/api/action", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({
            action,
            text: action === "add" ? input.value : (options.text ?? ""),
            index: options.index ?? null,
            direction: options.direction ?? null
          })
        });

        const result = await response.json();
        message.textContent =
          result.message || result.error || "Unknown response.";

        if (response.ok) {
          if (action === "add") input.value = "";

          if (["remove", "move", "start", "stop"].includes(action)) {
            cancelEdit();
          }
        }

        await refresh();
        return response.ok;

      } catch (error) {
        message.textContent = "Cannot reach HIDLab.";
        return false;

      } finally {
        requestPending = false;
      }
    }

    /**
     * Open the editor for a waiting passage.
     */
    function beginEdit(index) {
      if (index < 0 || index >= passages.length) return;

      editingIndex = index;
      document.getElementById("editInput").value = passages[index];
      document.getElementById("editPanel").hidden = false;
    }

    /**
     * Save changes to the selected passage.
     */
    async function saveEdit() {
      if (editingIndex === null) return;

      const saved = await sendAction("edit", {
        index: editingIndex,
        text: document.getElementById("editInput").value
      });

      if (saved) cancelEdit();
    }

    /**
     * Dismiss the passage editor without saving.
     */
    function cancelEdit() {
      editingIndex = null;
      document.getElementById("editPanel").hidden = true;
      document.getElementById("editInput").value = "";
    }

    /**
     * Create a button with its associated action.
     */
    function makeButton(label, callback, disabled = false, css = "") {
      const button = document.createElement("button");

      button.textContent = label;
      button.type = "button";
      button.disabled = disabled;
      button.className = css;
      button.addEventListener("click", callback);

      return button;
    }

    /**
     * Render the waiting queue and its management controls.
     */
    function renderQueue() {
      const list = document.getElementById("queue");
      list.replaceChildren();

      document.getElementById("emptyQueue").hidden =
        passages.length > 0;

      passages.forEach((text, index) => {
        const item = document.createElement("li");
        item.className = "queue-item";

        const preview = document.createElement("div");
        preview.className = "preview";

        preview.textContent =
          `Passage ${index + 1}: ${text.slice(0, 100)}` +
          (text.length > 100 ? "..." : "");

        const actions = document.createElement("div");
        actions.className = "actions";

        actions.appendChild(
          makeButton("Edit", () => beginEdit(index))
        );

        actions.appendChild(
          makeButton(
            "Up",
            () => sendAction("move", { index, direction: "up" }),
            index === 0
          )
        );

        actions.appendChild(
          makeButton(
            "Down",
            () => sendAction("move", { index, direction: "down" }),
            index === passages.length - 1
          )
        );

        actions.appendChild(
          makeButton(
            "Remove",
            () => sendAction("remove", { index }),
            false,
            "danger"
          )
        );

        item.append(preview, actions);
        list.appendChild(item);
      });
    }

    /**
     * Synchronize the interface with the device state.
     */
    async function refresh() {
      try {
        const response = await fetch("/api/status", {
          cache: "no-store"
        });

        if (!response.ok) throw new Error("Status request failed");

        const data = await response.json();

        document.getElementById("status").textContent =
          data.status.toUpperCase();

        passages = data.passages;
        renderQueue();

        if (editingIndex !== null &&
            editingIndex >= passages.length) {
          cancelEdit();
        }

        document.getElementById("start").disabled =
          data.status !== "idle" || data.queued === 0;

        document.getElementById("pause").disabled =
          data.status !== "typing";

        document.getElementById("resume").disabled =
          data.status !== "paused";

        document.getElementById("stop").disabled =
          data.status === "idle";

      } catch (error) {
        document.getElementById("status").textContent =
          "DISCONNECTED";

        for (const id of ["start", "pause", "resume", "stop"]) {
          document.getElementById(id).disabled = true;
        }
      }
    }

    refresh();
    setInterval(refresh, 1000);
  </script>
</body>
</html>
)hidlab";