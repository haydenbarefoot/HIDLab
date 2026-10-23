
/**
 * HIDLab firmware
 * Target: Arduino Nano ESP32
 *
 * Provides Wi-Fi connectivity and a local HTTP API for
 * managing passages and playback state.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

#include "wifi_credentials.h"
#include "PassageQueue.h"
#include "WebPage.h"

constexpr unsigned long WIFI_RETRY_INTERVAL_MS = 15000;
constexpr size_t MAX_REQUEST_BYTES = 32000;

WebServer server(80);
PassageQueue passageQueue;

unsigned long lastWifiAttempt = 0;
bool wasConnected = false;


/**
 * Check whether text contains only supported ASCII characters.
 * Printable ASCII, tabs, and line endings are permitted.
 */
bool isSupportedText(const String& text) {
    for (size_t i = 0; i < text.length(); ++i) {
        unsigned char character = text[i];

        if (character >= 32 && character <= 126) {
            continue;
        }

        if (character == '\n' ||
            character == '\r' ||
            character == '\t') {
            continue;
        }

        return false;
    }

    return true;
}


/**
 * Serialize a JSON document and send it to the HTTP client.
 */
void sendJson(int statusCode, const JsonDocument& document) {
    String response;
    serializeJson(document, response);

    server.send(
        statusCode,
        "application/json; charset=utf-8",
        response
    );
}


/**
 * Return an API error using the same response structure
 * as the Python simulator.
 */
void sendError(int statusCode, const char* message) {
    JsonDocument response;
    response["ok"] = false;
    response["error"] = message;

    sendJson(statusCode, response);
}


/**
 * Serve the browser-based HIDLab control interface.
 */
void handleRoot() {
    server.send_P(
        200,
        "text/html; charset=utf-8",
        INDEX_HTML
    );
}


/**
 * Return the current playback state and waiting passages.
 *
 * GET /api/status
 */
void handleStatus() {
    JsonDocument response;

    response["status"] = passageQueue.statusName();
    response["queued"] = passageQueue.waitingCount();

    JsonArray passages = response["passages"].to<JsonArray>();

    for (size_t i = 0; i < passageQueue.waitingCount(); ++i) {
        passages.add(passageQueue.passage(i));
    }

    sendJson(200, response);
}


/**
 * Execute a queue or playback command received from the browser.
 *
 * POST /api/action
 */
void handleAction() {
    if (!server.hasArg("plain")) {
        sendError(400, "Missing request body.");
        return;
    }

    String body = server.arg("plain");

    if (body.isEmpty() || body.length() > MAX_REQUEST_BYTES) {
        sendError(413, "Invalid request size.");
        return;
    }

    JsonDocument request;
    DeserializationError error = deserializeJson(request, body);

    if (error || !request.is<JsonObject>()) {
        sendError(400, "Invalid JSON request.");
        return;
    }

    const char* action = request["action"] | "";

    bool success = false;
    const char* message = "Unknown action.";

    if (strcmp(action, "add") == 0 ||
        strcmp(action, "edit") == 0) {

        if (!request["text"].is<const char*>()) {
            sendError(400, "Passage text must be a string.");
            return;
        }

        String text = request["text"].as<String>();

        if (!isSupportedText(text)) {
            sendError(400, "Only basic ASCII text is supported.");
            return;
        }

        if (strcmp(action, "add") == 0) {
            success = passageQueue.add(text);
            message = success
                ? "Passage added."
                : "Invalid passage or queue is full.";
        } else {
            if (!request["index"].is<int>()) {
                sendError(400, "Invalid queue position.");
                return;
            }

            int index = request["index"].as<int>();

            success = index >= 0 &&
                passageQueue.edit(
                    static_cast<size_t>(index),
                    text
                );

            message = success
                ? "Passage updated."
                : "Invalid passage or queue position.";
        }

    } else if (strcmp(action, "remove") == 0) {

        if (!request["index"].is<int>()) {
            sendError(400, "Invalid queue position.");
            return;
        }

        int index = request["index"].as<int>();

        success = index >= 0 &&
            passageQueue.remove(static_cast<size_t>(index));

        message = success
            ? "Passage removed."
            : "Invalid queue position.";

    } else if (strcmp(action, "move") == 0) {

        if (!request["index"].is<int>() ||
            !request["direction"].is<const char*>()) {
            sendError(400, "Invalid move parameters.");
            return;
        }

        int index = request["index"].as<int>();
        String direction = request["direction"].as<String>();

        int offset = 0;

        if (direction == "up") {
            offset = -1;
        } else if (direction == "down") {
            offset = 1;
        }

        success = index >= 0 && offset != 0 &&
            passageQueue.move(
                static_cast<size_t>(index),
                offset
            );

        message = success
            ? "Queue order updated."
            : "Invalid move.";

    } else if (strcmp(action, "start") == 0) {

        success = passageQueue.start();
        message = success ? "Started." : "Cannot start.";

    } else if (strcmp(action, "pause") == 0) {

        success = passageQueue.pause();
        message = success ? "Paused." : "Cannot pause.";

    } else if (strcmp(action, "resume") == 0) {

        success = passageQueue.resume();
        message = success ? "Resumed." : "Cannot resume.";

    } else if (strcmp(action, "stop") == 0) {

        success = passageQueue.stop();
        message = success
            ? "Stopped. Passage returned to queue."
            : "Cannot stop.";
    }

    JsonDocument response;
    response["ok"] = success;
    response["message"] = message;

    sendJson(success ? 200 : 400, response);
}


/**
 * Register the HTTP routes and start listening for requests.
 */
void configureServer() {
    server.on("/", HTTP_GET, handleRoot);
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/action", HTTP_POST, handleAction);

    server.onNotFound([]() {
        sendError(404, "Not found.");
    });

    server.begin();
}


/**
 * Begin connecting to the configured Wi-Fi network.
 */
void connectWifi() {
    Serial.println("Connecting to Wi-Fi...");

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    lastWifiAttempt = millis();
}


/**
 * Monitor Wi-Fi state and periodically retry disconnections.
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
 * Initialize serial diagnostics, Wi-Fi, and HTTP services.
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
 * Maintain network connectivity and process HTTP requests.
 */
void loop() {
    maintainWifi();

    if (WiFi.status() == WL_CONNECTED) {
        server.handleClient();
    }

    delay(10);
}
