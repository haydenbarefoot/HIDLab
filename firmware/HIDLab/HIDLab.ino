/**
 * HIDLab firmware
 * Target: Arduino Nano ESP32
 *
 * Initial Wi-Fi connectivity and HTTP server implementation.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "PassageQueue.h"

#include "wifi_credentials.h"

constexpr unsigned long WIFI_RETRY_INTERVAL_MS = 15000;

WebServer server(80);
PassageQueue passageQueue;


unsigned long lastWifiAttempt = 0;
bool wasConnected = false;

const char INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>HIDLab</title>
  <style>
    body {
      background: #111827;
      color: #f9fafb;
      font-family: system-ui, sans-serif;
      margin: 0;
      padding: 24px;
    }

    main {
      max-width: 600px;
      margin: 40px auto;
    }

    .panel {
      background: #1f2937;
      border-radius: 12px;
      padding: 24px;
      margin-top: 20px;
    }

    p {
      color: #9ca3af;
    }

    #status {
      font-weight: bold;
    }
  </style>
</head>
<body>
  <main>
    <h1>HIDLab</h1>
    <p>USB HID Research Platform</p>

    <section class="panel">
      <h2>Device Status</h2>
      <div id="status">Connecting...</div>
      <p>Arduino Nano ESP32 — Wi-Fi firmware test</p>
    </section>
  </main>

  <script>
    /**
     * Fetch the current device status and update the page.
     */
    async function refreshStatus() {
      try {
        const response = await fetch("/api/status");

        if (!response.ok) {
          throw new Error("Status request failed");
        }

        const data = await response.json();

        document.getElementById("status").textContent =
          data.status.toUpperCase();

      } catch (error) {
        document.getElementById("status").textContent =
          "DISCONNECTED";
      }
    }

    refreshStatus();
    setInterval(refreshStatus, 2000);
  </script>
</body>
</html>
)rawliteral";


/**
 * Return the current queue and playback status.
 */
void handleStatus() {
    String response = "{\"status\":\"";
    response += passageQueue.statusName();
    response += "\",\"queued\":";
    response += passageQueue.waitingCount();
    response += ",\"passages\":[]}";

    server.send(200, "application/json", response);
}

/**
 * Serve the temporary firmware validation webpage.
 */
void handleRoot() {
  server.send(200, "text/html; charset=utf-8", INDEX_HTML);
}


/**
 * Register the HTTP routes used by the device.
 */
void configureServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);

  server.onNotFound([]() {
    server.send(404, "application/json",
                "{\"error\":\"Not found\"}");
  });

  server.begin();
}


/**
 * Initiate a Wi-Fi connection without blocking the main loop.
 */
void connectWifi() {
  Serial.println("Connecting to Wi-Fi...");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastWifiAttempt = millis();
}


/**
 * Maintain the Wi-Fi connection and report changes.
 */
void maintainWifi() {
  bool connected = WiFi.status() == WL_CONNECTED;

  if (connected && !wasConnected) {
    Serial.println("Wi-Fi connected.");
    Serial.print("HIDLab address: http://");
    Serial.println(WiFi.localIP());
  }

  if (!connected && wasConnected) {
    Serial.println("Wi-Fi disconnected.");
  }

  wasConnected = connected;

  if (!connected &&
      millis() - lastWifiAttempt >= WIFI_RETRY_INTERVAL_MS) {
    connectWifi();
  }
}


/**
 * Initialize USB serial diagnostics, Wi-Fi, and the HTTP server.
 */
void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  configureServer();
  connectWifi();

  Serial.println("HIDLab initialized.");
}


/**
 * Handle network connectivity and incoming HTTP requests.
 */
void loop() {
  maintainWifi();

  if (WiFi.status() == WL_CONNECTED) {
    server.handleClient();
  }

  delay(10);
}
