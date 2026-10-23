
/**
 * HIDLab firmware
 * Target: Arduino Nano ESP32
 *
 * Wi-Fi-controlled USB HID keyboard research platform.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <USB.h>
#include <USBHIDKeyboard.h>

#include "PassageQueue.h"
#include "NetworkManager.h"
#include "WebPage.h"

constexpr unsigned long KEY_INTERVAL_MS = 150;
constexpr unsigned long SESSION_TIMEOUT_MS = 30000;
constexpr size_t MAX_REQUEST_BYTES = 32000;

WebServer server(80);
PassageQueue passageQueue;
NetworkManager networkManager;
USBHIDKeyboard Keyboard;

unsigned long lastKeyTime = 0;
unsigned long lastSessionActivity = 0;

String activeSession;


/** Check that a passage contains only supported ASCII text. */
bool isSupportedText(const String& text) {
    for (size_t i = 0; i < text.length(); ++i) {
        const unsigned char ch = text[i];

        if (ch >= 32 && ch <= 126) {
            continue;
        }

        if (ch == '\n' || ch == '\r' || ch == '\t') {
            continue;
        }

        return false;
    }

    return true;
}


/** Send a JSON document as an HTTP response. */
void sendJson(int statusCode, const JsonDocument& document) {
    String body;
    serializeJson(document, body);

    server.send(
        statusCode,
        "application/json; charset=utf-8",
        body
    );
}


/** Send a consistent JSON error response. */
void sendError(int statusCode, const char* message) {
    JsonDocument response;
    response["ok"] = false;
    response["error"] = message;
    sendJson(statusCode, response);
}


/** Parse the incoming JSON request body. */
bool readRequest(JsonDocument& request) {
    if (!server.hasArg("plain")) {
        sendError(400, "Missing request body.");
        return false;
    }

    const String body = server.arg("plain");

    if (body.isEmpty() || body.length() > MAX_REQUEST_BYTES) {
        sendError(413, "Invalid request size.");
        return false;
    }

    if (deserializeJson(request, body) ||
        !request.is<JsonObject>()) {
        sendError(400, "Invalid JSON request.");
        return false;
    }

    return true;
}


/**
 * Check whether the browser holds the active control session.
 *
 * Sessions prevent simultaneous commands from different browsers.
 * They are not authentication against other devices on the LAN.
 */
bool authorizeSession() {
    const String token = server.header("X-HIDLab-Session");

    if (token.isEmpty() || token != activeSession) {
        sendError(423, "Another controller owns this session.");
        return false;
    }

    lastSessionActivity = millis();
    return true;
}


/**
 * Acquire or renew the single-controller session.
 *
 * An inactive session expires after 30 seconds.
 */
void handleSession() {
    JsonDocument request;

    if (!readRequest(request)) {
        return;
    }

    const String token = request["token"] | "";

    if (token.length() < 16 || token.length() > 128) {
        sendError(400, "Invalid session token.");
        return;
    }

    const bool expired =
        millis() - lastSessionActivity >= SESSION_TIMEOUT_MS;

    if (!activeSession.isEmpty() &&
        activeSession != token &&
        !expired) {
        sendError(423, "Another controller is connected.");
        return;
    }

    activeSession = token;
    lastSessionActivity = millis();

    JsonDocument response;
    response["ok"] = true;
    sendJson(200, response);
}


/** Serve the control webpage from firmware flash. */
void handleRoot() {
    server.send_P(
        200,
        "text/html; charset=utf-8",
        INDEX_HTML
    );
}


/** Return the current queue and playback state. */
void handleStatus() {
    JsonDocument response;

    response["status"] = passageQueue.statusName();
    response["queued"] = passageQueue.waitingCount();
    response["network"] = networkManager.currentSSID();
    response["ip"] = networkManager.address();
    response["setup_mode"] = networkManager.setupMode();

    JsonArray passages =
        response["passages"].to<JsonArray>();

    for (size_t i = 0; i < passageQueue.waitingCount(); ++i) {
        passages.add(passageQueue.passage(i));
    }

    sendJson(200, response);
}


/** Execute passage queue and playback commands. */
void handleAction() {
    if (!authorizeSession()) {
        return;
    }

    JsonDocument request;

    if (!readRequest(request)) {
        return;
    }

    const String action = request["action"] | "";
    const int index = request["index"].is<int>()
        ? request["index"].as<int>()
        : -1;

    bool success = false;
    const char* message = "Unknown action.";

    if (action == "add" || action == "edit") {
        if (!request["text"].is<const char*>()) {
            sendError(400, "Text must be a string.");
            return;
        }

        const String text = request["text"].as<String>();

        if (!isSupportedText(text)) {
            sendError(400, "Only ASCII text is supported.");
            return;
        }

        if (action == "add") {
            success = passageQueue.add(text);
        } else if (index >= 0) {
            success = passageQueue.edit(index, text);
        }

        message = success
            ? "Passage saved."
            : "Invalid passage or queue position.";

    } else if (action == "remove") {
        success = index >= 0 && passageQueue.remove(index);
        message = success
            ? "Passage removed."
            : "Invalid queue position.";

    } else if (action == "move") {
        const String direction = request["direction"] | "";

        const int offset =
            direction == "up" ? -1 :
            direction == "down" ? 1 : 0;

        success = index >= 0 && offset != 0 &&
                  passageQueue.move(index, offset);

        message = success
            ? "Queue order updated."
            : "Invalid move.";

    } else if (action == "start") {
        success = passageQueue.start();
        message = success ? "Started." : "Cannot start.";

        if (success) {
            lastKeyTime = millis();
        }

    } else if (action == "pause") {
        success = passageQueue.pause();
        message = success ? "Paused." : "Cannot pause.";

    } else if (action == "resume") {
        success = passageQueue.resume();
        message = success ? "Resumed." : "Cannot resume.";

        if (success) {
            lastKeyTime = millis();
        }

    } else if (action == "stop") {
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


/** Return saved Wi-Fi network names without their passwords. */
void handleNetworks() {
    JsonDocument response;

    JsonArray networks =
        response["networks"].to<JsonArray>();

    for (int i = 0; i < networkManager.networkCount(); ++i) {
        networks.add(networkManager.networkName(i));
    }

    sendJson(200, response);
}


/** Add, update, or remove a saved Wi-Fi profile. */
void handleNetworkAction() {
    if (!authorizeSession()) {
        return;
    }

    JsonDocument request;

    if (!readRequest(request)) {
        return;
    }

    const String action = request["action"] | "";
    bool success = false;

    if (action == "add") {
        if (!request["ssid"].is<const char*>() ||
            !request["password"].is<const char*>()) {
            sendError(400, "Invalid network configuration.");
            return;
        }

        success = networkManager.addNetwork(
            request["ssid"].as<String>(),
            request["password"].as<String>()
        );

    } else if (action == "remove") {
        const int index = request["index"].is<int>()
            ? request["index"].as<int>()
            : -1;

        success = networkManager.removeNetwork(index);
    }

    JsonDocument response;
    response["ok"] = success;
    response["message"] = success
        ? "Network configuration saved."
        : "Network configuration rejected.";

    sendJson(success ? 200 : 400, response);
}


/** Register HTTP endpoints used by the HIDLab interface. */
void configureServer() {
    const char* headers[] = {"X-HIDLab-Session"};
    server.collectHeaders(headers, 1);

    server.on("/", HTTP_GET, handleRoot);
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/session", HTTP_POST, handleSession);
    server.on("/api/action", HTTP_POST, handleAction);
    server.on("/api/networks", HTTP_GET, handleNetworks);
    server.on(
        "/api/networks/action",
        HTTP_POST,
        handleNetworkAction
    );

    server.onNotFound([]() {
        sendError(404, "Not found.");
    });

    server.begin();
}


/**
 * Send the next ASCII character while playback is active.
 *
 * The current queue implementation advances before USB
 * delivery is confirmed. USB recovery remains provisional.
 */
void processKeyboard() {
    if (passageQueue.state() != PassageQueue::State::Typing) {
        return;
    }

    const unsigned long now = millis();

    if (now - lastKeyTime < KEY_INTERVAL_MS) {
        return;
    }

    lastKeyTime = now;

    char character;

    if (!passageQueue.nextCharacter(character)) {
        return;
    }

    // Ignore carriage returns so Windows CRLF becomes
    // a single Enter event rather than two line breaks.
    if (character == '\r') {
        return;
    }

    Keyboard.write(static_cast<uint8_t>(character));
}


/** Initialize the USB device and network services. */
void setup() {
    Serial.begin(115200);

    Keyboard.begin();
    USB.begin();

    configureServer();
    networkManager.begin();

    Serial.println("HIDLab initialized.");
}


/** Service network, HTTP, and keyboard operations. */
void loop() {
    networkManager.update();

    if (networkManager.connected() ||
        networkManager.setupMode()) {
        server.handleClient();
    }

    processKeyboard();
    delay(1);
}
