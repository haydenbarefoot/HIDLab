
/** HIDLab: Wi-Fi-controlled USB HID keyboard for Arduino Nano ESP32. */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <USB.h>
#include <USBHIDKeyboard.h>

#include "PassageQueue.h"
#include "NetworkManager.h"
#include "HumanTypingEngine.h"
#include "WebPage.h"

constexpr unsigned long SESSION_LEASE_MS = 30000;
constexpr size_t MAX_REQUEST_BYTES = 32000;

WebServer server(80);
USBHIDKeyboard Keyboard;
PassageQueue queue;
NetworkManager networks;
HumanTypingEngine typing;

volatile bool usbReady = false;
volatile bool usbLost = false;
bool pausedByUsb = false;

String controllerToken;
unsigned long lastControllerAt = 0;
uint32_t networkGeneration = 0;

/** Track USB host enumeration, disconnection, and suspend events. */
static void usbEvent(void*, esp_event_base_t base, int32_t event, void*) {
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

void sendJson(int code, const JsonDocument& data) {
    String result;
    serializeJson(data, result);
    server.sendHeader("Cache-Control", "no-store");
    server.send(code, "application/json; charset=utf-8", result);
}

void sendError(int code, const char* message) {
    JsonDocument data;
    data["ok"] = false;
    data["error"] = message;
    sendJson(code, data);
}

/** Validate and parse a JSON object from the request body. */
bool readJson(JsonDocument& data) {
    if (!server.hasArg("plain")) {
        sendError(400, "Missing request body.");
        return false;
    }

    const String body = server.arg("plain");
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

/** Restrict APIs to the current browser controller. */
bool authorized() {
    const String token = server.header("X-HIDLab-Session");
    if (controllerToken.isEmpty() ||
        millis() - lastControllerAt >= SESSION_LEASE_MS ||
        token != controllerToken) {
        sendError(423, "Controller unavailable. Reconnect from the webpage.");
        return false;
    }
    lastControllerAt = millis();
    return true;
}

void handleSession() {
    JsonDocument request;
    if (!readJson(request)) return;

    if (!request["token"].is<const char*>()) {
        sendError(400, "Missing session token.");
        return;
    }
    const String token = request["token"].as<String>();
    if (token.length() < 32 || token.length() > 128) {
        sendError(400, "Invalid session token.");
        return;
    }
    if (!controllerToken.isEmpty() && token != controllerToken &&
        millis() - lastControllerAt < SESSION_LEASE_MS) {
        sendError(423, "Another browser has control.");
        return;
    }

    controllerToken = token;
    lastControllerAt = millis();
    JsonDocument response;
    response["ok"] = true;
    sendJson(200, response);
}

void handleRelease() {
    if (!authorized()) return;
    controllerToken = "";
    JsonDocument response;
    response["ok"] = true;
    sendJson(200, response);
}

void handleRoot() {
    server.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

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
    JsonArray passages = response["passages"].to<JsonArray>();
    for (size_t i = 0; i < queue.waitingCount(); ++i) {
        passages.add(queue.passage(i));
    }
    sendJson(200, response);
}

/** Operate the passage queue without blocking network requests. */
void handleAction() {
    if (!authorized()) return;
    JsonDocument request;
    if (!readJson(request)) return;

    const String command = request["action"] | "";
    const int index = request["index"].is<int>()
        ? request["index"].as<int>() : -1;

    bool ok = false;
    const char* message = "Unknown action.";

    if (command == "add" || command == "edit") {
        if (!request["text"].is<const char*>()) {
            sendError(400, "Text must be a string.");
            return;
        }
        const String text = request["text"].as<String>();
        ok = command == "add" ? queue.add(text)
            : (index >= 0 && queue.edit(index, text));
        message = ok ? "Passage saved." : "Invalid passage or queue full.";

    } else if (command == "remove") {
        ok = index >= 0 && queue.remove(index);
        message = ok ? "Passage removed." : "Invalid queue position.";

    } else if (command == "move") {
        const String dir = request["direction"] | "";
        const int offset = dir == "up" ? -1 : dir == "down" ? 1 : 0;
        ok = index >= 0 && offset != 0 && queue.move(index, offset);
        message = ok ? "Queue reordered." : "Invalid move.";

    } else if (command == "start") {
        ok = usbReady && queue.start();
        if (ok) {
            pausedByUsb = false;
            typing.beginPassage();
        }
        message = ok ? "Started." : "Cannot start. Check queue and USB.";

    } else if (command == "pause") {
        ok = queue.pause();
        message = ok ? "Paused." : "Nothing playing.";

    } else if (command == "resume") {
        ok = usbReady && queue.resume();
        if (ok) {
            pausedByUsb = false;
            typing.resume();
        }
        message = ok ? "Resumed." : "Cannot resume. Check USB.";

    } else if (command == "stop") {
        // A correction may be halfway through when Stop is pressed.
        if (typing.hasUncorrectedTypo() && usbReady) {
            Keyboard.write(KEY_BACKSPACE);
        }
        ok = queue.stop();
        if (ok) {
            pausedByUsb = false;
            typing.reset();
        }
        message = ok ? "Stopped; passage returned to queue." : "Already idle.";
    }

    JsonDocument response;
    response["ok"] = ok;
    response["message"] = message;
    sendJson(ok ? 200 : 400, response);
}

/** Retrieve or persist the text typing profile. */
void handleSettings() {
    if (!authorized()) return;
    JsonDocument response;
    response["mode"] = typing.modeName();
    response["wpm"] = typing.wpm();
    response["typo_rate"] = typing.typoRate();
    sendJson(200, response);
}

void handleSaveSettings() {
    if (!authorized()) return;
    if (queue.state() != PassageQueue::State::Idle) {
        sendError(409, "Stop playback before changing typing settings.");
        return;
    }
    JsonDocument request;
    if (!readJson(request)) return;
    if (!request["mode"].is<const char*>() ||
        !request["wpm"].is<int>() ||
        !request["typo_rate"].is<int>()) {
        sendError(400, "Invalid typing settings.");
        return;
    }

    const bool ok = typing.configure(
        request["mode"].as<String>(),
        request["wpm"].as<int>(),
        request["typo_rate"].as<int>()
    );
    JsonDocument response;
    response["ok"] = ok;
    response["message"] = ok ? "Typing settings saved." : "Invalid typing settings.";
    sendJson(ok ? 200 : 400, response);
}

void handleNetworks() {
    if (!authorized()) return;
    JsonDocument response;
    JsonArray saved = response["networks"].to<JsonArray>();
    for (int i = 0; i < networks.count(); ++i) saved.add(networks.name(i));
    sendJson(200, response);
}

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
            ? request["index"].as<int>() : -1;
        ok = networks.remove(index);
    }

    JsonDocument response;
    response["ok"] = ok;
    response["message"] = ok
        ? "Network saved. Connection may change." : "Network change rejected.";
    sendJson(ok ? 200 : 400, response);
}

void configureServer() {
    const char* headers[] = {"X-HIDLab-Session"};
    server.collectHeaders(headers, 1);
    server.on("/", HTTP_GET, handleRoot);
    server.on("/api/session", HTTP_POST, handleSession);
    server.on("/api/release", HTTP_POST, handleRelease);
    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/action", HTTP_POST, handleAction);
    server.on("/api/settings", HTTP_GET, handleSettings);
    server.on("/api/settings", HTTP_POST, handleSaveSettings);
    server.on("/api/networks", HTTP_GET, handleNetworks);
    server.on("/api/networks/action", HTTP_POST, handleNetworkAction);
    server.onNotFound([]() { sendError(404, "Not found."); });
    server.begin();
}

void maintainUsb() {
    if (!usbLost) return;
    usbLost = false;
    if (queue.state() == PassageQueue::State::Typing && queue.pause()) {
        pausedByUsb = true;
    }
}

void setup() {
    Serial.begin(115200);
    USB.onEvent(usbEvent);
    Keyboard.begin();
    USB.begin();
    typing.begin();
    configureServer();
    networks.begin();
    networkGeneration = networks.generation();
    Serial.println("HIDLab ready");
}

void loop() {
    maintainUsb();
    networks.update();
    if (networkGeneration != networks.generation()) {
        networkGeneration = networks.generation();
        controllerToken = "";
    }
    if (networks.connected() || networks.setupMode()) server.handleClient();
    maintainUsb();
    typing.update(queue, Keyboard, usbReady);
    delay(1);
}
