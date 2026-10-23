
/** HIDLab: Wi-Fi-controlled USB HID keyboard for Arduino Nano ESP32. */

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
constexpr unsigned long SESSION_LEASE_MS = 30000;
constexpr size_t MAX_REQUEST_BYTES = 32000;

WebServer server(80);
USBHIDKeyboard Keyboard;
PassageQueue queue;
NetworkManager networks;

volatile bool usbReady = false;
volatile bool usbLost = false;

bool pausedByUsb = false;
unsigned long lastKeyAt = 0;

String controllerToken;
unsigned long lastControllerAt = 0;
uint32_t networkGeneration = 0;


/** Track USB enumeration, disconnection, suspend, and resume events. */
static void usbEvent(
    void*,
    esp_event_base_t base,
    int32_t event,
    void*
) {
    if (base != ARDUINO_USB_EVENTS) return;

    if (event == ARDUINO_USB_STARTED_EVENT ||
        event == ARDUINO_USB_RESUME_EVENT) {

        usbReady = true;

    } else if (event == ARDUINO_USB_STOPPED_EVENT ||
               event == ARDUINO_USB_SUSPEND_EVENT) {

        usbReady = false;
        usbLost = true;
    }
}


/** Serialize a JSON document to an HTTP response. */
void sendJson(int code, const JsonDocument& data) {
    String result;
    serializeJson(data, result);

    server.sendHeader("Cache-Control", "no-store");
    server.send(
        code,
        "application/json; charset=utf-8",
        result
    );
}


void sendError(int code, const char* message) {
    JsonDocument data;
    data["ok"] = false;
    data["error"] = message;

    sendJson(code, data);
}


/** Validate and parse an incoming JSON request. */
bool readJson(JsonDocument& data) {
    if (!server.hasArg("plain")) {
        sendError(400, "Missing request body.");
        return false;
    }

    String body = server.arg("plain");

    if (body.isEmpty() || body.length() > MAX_REQUEST_BYTES) {
        sendError(413, "Invalid request size.");
        return false;
    }

    if (deserializeJson(data, body) || !data.is<JsonObject>()) {
        sendError(400, "Invalid JSON.");
        return false;
    }

    return true;
}


/** Require a valid, unexpired browser control session. */
bool authorized() {
    const String token = server.header("X-HIDLab-Session");

    const bool expired =
        millis() - lastControllerAt >= SESSION_LEASE_MS;

    if (controllerToken.isEmpty() ||
        expired ||
        token != controllerToken) {

        sendError(
            423,
            "Controller session unavailable. Reconnect from the control page."
        );

        return false;
    }

    lastControllerAt = millis();
    return true;
}


/** Acquire or renew the single browser-controller session. */
void handleSession() {
    JsonDocument request;

    if (!readJson(request)) return;

    if (!request["token"].is<const char*>()) {
        sendError(400, "Missing session token.");
        return;
    }

    String token = request["token"].as<String>();

    if (token.length() < 32 || token.length() > 128) {
        sendError(400, "Invalid session token.");
        return;
    }

    if (!controllerToken.isEmpty() &&
        token != controllerToken &&
        millis() - lastControllerAt < SESSION_LEASE_MS) {

        sendError(423, "Another device has control.");
        return;
    }

    controllerToken = token;
    lastControllerAt = millis();

    JsonDocument response;
    response["ok"] = true;

    sendJson(200, response);
}


/** Release control without stopping the current passage. */
void handleRelease() {
    if (!authorized()) return;

    controllerToken = "";

    JsonDocument response;
    response["ok"] = true;

    sendJson(200, response);
}


void handleRoot() {
    server.send_P(
        200,
        "text/html; charset=utf-8",
        INDEX_HTML
    );
}


/** Return queue, USB, and network status to the controller. */
void handleStatus() {
    if (!authorized()) return;

    JsonDocument response;

    response["status"] = queue.statusName();
    response["queued"] = queue.waitingCount();
    response["usb_ready"] = static_cast<bool>(usbReady);
    response["usb_paused"] = pausedByUsb;
    response["network"] = networks.currentSSID();
    response["ip"] = networks.address();
    response["setup_mode"] = networks.setupMode();
    response["network_exhausted"] = networks.exhausted();

    JsonArray passages =
        response["passages"].to<JsonArray>();

    for (size_t i = 0; i < queue.waitingCount(); ++i) {
        passages.add(queue.passage(i));
    }

    sendJson(200, response);
}


/** Apply validated queue or playback commands. */
void handleAction() {
    if (!authorized()) return;

    JsonDocument request;
    if (!readJson(request)) return;

    const String command = request["action"] | "";

    const int index = request["index"].is<int>()
        ? request["index"].as<int>()
        : -1;

    bool ok = false;
    const char* message = "Unknown action.";

    if (command == "add" || command == "edit") {
        if (!request["text"].is<const char*>()) {
            sendError(400, "Text must be a string.");
            return;
        }

        const String text = request["text"].as<String>();

        ok = command == "add"
            ? queue.add(text)
            : (index >= 0 && queue.edit(index, text));

        message = ok
            ? "Passage saved."
            : "Invalid ASCII passage or queue is full.";

    } else if (command == "remove") {
        ok = index >= 0 && queue.remove(index);

        message = ok
            ? "Passage removed."
            : "Invalid queue position.";

    } else if (command == "move") {
        const String dir = request["direction"] | "";

        const int offset =
            dir == "up" ? -1 :
            dir == "down" ? 1 : 0;

        ok = index >= 0 &&
             offset != 0 &&
             queue.move(index, offset);

        message = ok
            ? "Queue reordered."
            : "Invalid move.";

    } else if (command == "start") {
        ok = usbReady && queue.start();

        if (ok) {
            pausedByUsb = false;
            lastKeyAt = millis();
        }

        message = ok
            ? "Started."
            : "Cannot start: check queue and USB connection.";

    } else if (command == "pause") {
        ok = queue.pause();

        message = ok
            ? "Paused."
            : "Not currently typing.";

    } else if (command == "resume") {
        ok = usbReady && queue.resume();

        if (ok) {
            pausedByUsb = false;
            lastKeyAt = millis();
        }

        message = ok
            ? "Resumed."
            : "Cannot resume: check USB connection.";

    } else if (command == "stop") {
        ok = queue.stop();

        if (ok) {
            pausedByUsb = false;
        }

        message = ok
            ? "Stopped; passage returned to queue."
            : "Already idle.";
    }

    JsonDocument response;
    response["ok"] = ok;
    response["message"] = message;

    sendJson(ok ? 200 : 400, response);
}


/** List saved Wi-Fi network names, excluding passwords. */
void handleNetworks() {
    if (!authorized()) return;

    JsonDocument response;

    JsonArray saved =
        response["networks"].to<JsonArray>();

    for (int i = 0; i < networks.count(); ++i) {
        saved.add(networks.name(i));
    }

    sendJson(200, response);
}


/** Modify persistent network profiles through the API. */
void handleNetworkAction() {
    if (!authorized()) return;

    JsonDocument request;
    if (!readJson(request)) return;

    const String command = request["action"] | "";
    bool ok = false;

    if (command == "add") {
        if (!request["ssid"].is<const char*>() ||
            !request["password"].is<const char*>()) {

            sendError(400, "Invalid Wi-Fi fields.");
            return;
        }

        ok = networks.addOrUpdate(
            request["ssid"].as<String>(),
            request["password"].as<String>()
        );

    } else if (command == "remove") {
        const int index = request["index"].is<int>()
            ? request["index"].as<int>()
            : -1;

        ok = networks.remove(index);
    }

    JsonDocument response;

    response["ok"] = ok;
    response["message"] = ok
        ? "Network saved. Connection may change."
        : "Network change rejected.";

    sendJson(ok ? 200 : 400, response);
}


/** Register HTTP endpoints. */
void configureServer() {
    const char* headers[] = {
        "X-HIDLab-Session"
    };

    server.collectHeaders(headers, 1);

    server.on("/", HTTP_GET, handleRoot);
    server.on("/api/session", HTTP_POST, handleSession);
    server.on("/api/release", HTTP_POST, handleRelease);
    server.on("/api/status", HTTP_GET, handleStatus);
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


/** Pause active output when USB disconnects or suspends. */
void maintainUsb() {
    if (!usbLost) return;

    usbLost = false;

    if (queue.state() == PassageQueue::State::Typing) {
        if (queue.pause()) {
            pausedByUsb = true;
        }
    }
}


/** Send one queued ASCII character to the USB keyboard interface. */
void processKeyboard() {
    if (!usbReady ||
        queue.state() != PassageQueue::State::Typing) {
        return;
    }

    if (millis() - lastKeyAt < KEY_INTERVAL_MS) {
        return;
    }

    lastKeyAt = millis();

    char character;

    if (!queue.peek(character)) {
        return;
    }

    if (Keyboard.write(static_cast<uint8_t>(character)) == 1 &&
        usbReady) {

        queue.advance();

    } else if (!usbReady) {
        maintainUsb();
    }
}


void setup() {
    Serial.begin(115200);

    USB.onEvent(usbEvent);
    Keyboard.begin();
    USB.begin();

    configureServer();
    networks.begin();

    networkGeneration = networks.generation();

    Serial.println("HIDLab ready");
}


void loop() {
    maintainUsb();
    networks.update();

    if (networkGeneration != networks.generation()) {
        // Network changes invalidate sessions from the previous address.
        networkGeneration = networks.generation();
        controllerToken = "";
    }

    if (networks.connected() || networks.setupMode()) {
        server.handleClient();
    }

    maintainUsb();
    processKeyboard();

    delay(1);
}
