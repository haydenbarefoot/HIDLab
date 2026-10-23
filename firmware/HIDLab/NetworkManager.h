
#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <ESPmDNS.h>

#include "wifi_credentials.h"

#ifndef HIDLAB_SETUP_PASSWORD
#define HIDLAB_SETUP_PASSWORD "ChangeThisPassword2026"
#endif

class NetworkManager {
public:
    static constexpr int MAX_NETWORKS = 4;

    struct Profile {
        String ssid;
        String password;
    };

    void begin() {
        preferences_.begin("hidlab-wifi", false);
        loadProfiles();

        // Import the original firmware credentials on first setup.
        if (count_ == 0 && strlen(WIFI_SSID) > 0) {
            addNetwork(WIFI_SSID, WIFI_PASSWORD);
        }

        WiFi.mode(WIFI_STA);
        startNextConnection();
    }

    /**
     * Service connection attempts without blocking HTTP processing.
     */
    void update() {
        const bool connected = WiFi.status() == WL_CONNECTED;

        if (connected) {
            if (!wasConnected_) {
                wasConnected_ = true;
                connecting_ = false;
                attempts_ = 0;

                Serial.print("HIDLab: http://");
                Serial.println(WiFi.localIP());

                if (MDNS.begin("hidlab")) {
                    MDNS.addService("http", "tcp", 80);
                }

                if (apActive_) {
                    WiFi.softAPdisconnect(true);
                    apActive_ = false;
                }
            }
            return;
        }

        if (wasConnected_) {
            wasConnected_ = false;
            connecting_ = false;
            MDNS.end();
            attempts_ = 0;
            profileIndex_ = 0;
        }

        if (connecting_) {
            if (millis() - attemptStarted_ < CONNECTION_TIMEOUT_MS) {
                return;
            }

            WiFi.disconnect();
            connecting_ = false;
            ++attempts_;

            // Give the first network three attempts and each
            // subsequent saved network one attempt.
            if (profileIndex_ == 0 && attempts_ < 3) {
                startNextConnection();
                return;
            }

            ++profileIndex_;
            attempts_ = 0;
        }

        if (profileIndex_ < count_) {
            startNextConnection();
        } else if (!apActive_) {
            startAccessPoint();
        }
    }

    /**
     * Store a network profile in flash and restart discovery.
     */
    bool addNetwork(const String& ssid, const String& password) {
        if (ssid.isEmpty() || ssid.length() > 32 ||
            password.length() > 63) {
            return false;
        }

        // Update an existing profile rather than duplicating it.
        for (int i = 0; i < count_; ++i) {
            if (profiles_[i].ssid == ssid) {
                profiles_[i].password = password;
                saveProfiles();
                restartConnections();
                return true;
            }
        }

        if (count_ >= MAX_NETWORKS) {
            return false;
        }

        profiles_[count_++] = {ssid, password};
        saveProfiles();
        restartConnections();
        return true;
    }

    /** Remove a saved network by index. */
    bool removeNetwork(int index) {
        if (index < 0 || index >= count_) {
            return false;
        }

        for (int i = index; i < count_ - 1; ++i) {
            profiles_[i] = profiles_[i + 1];
        }

        profiles_[--count_] = {"", ""};
        saveProfiles();
        restartConnections();
        return true;
    }

    int networkCount() const {
        return count_;
    }

    String networkName(int index) const {
        return index >= 0 && index < count_
            ? profiles_[index].ssid
            : "";
    }

    bool connected() const {
        return WiFi.status() == WL_CONNECTED;
    }

    bool setupMode() const {
        return apActive_;
    }

    String address() const {
        if (connected()) {
            return WiFi.localIP().toString();
        }

        if (apActive_) {
            return WiFi.softAPIP().toString();
        }

        return "";
    }

    String currentSSID() const {
        return connected() ? WiFi.SSID() : "";
    }

private:
    static constexpr unsigned long CONNECTION_TIMEOUT_MS = 8000;

    Preferences preferences_;
    Profile profiles_[MAX_NETWORKS];

    int count_ = 0;
    int profileIndex_ = 0;
    int attempts_ = 0;

    bool wasConnected_ = false;
    bool connecting_ = false;
    bool apActive_ = false;

    unsigned long attemptStarted_ = 0;

    /** Read saved Wi-Fi profiles from nonvolatile storage. */
    void loadProfiles() {
        count_ = constrain(
            preferences_.getInt("count", 0),
            0,
            MAX_NETWORKS
        );

        for (int i = 0; i < count_; ++i) {
            String prefix = "wifi" + String(i);
            profiles_[i].ssid =
                preferences_.getString((prefix + "s").c_str(), "");
            profiles_[i].password =
                preferences_.getString((prefix + "p").c_str(), "");
        }
    }

    /** Replace saved profiles without exposing passwords via HTTP. */
    void saveProfiles() {
        preferences_.putInt("count", count_);

        for (int i = 0; i < MAX_NETWORKS; ++i) {
            String prefix = "wifi" + String(i);

            preferences_.putString(
                (prefix + "s").c_str(),
                i < count_ ? profiles_[i].ssid : ""
            );

            preferences_.putString(
                (prefix + "p").c_str(),
                i < count_ ? profiles_[i].password : ""
            );
        }
    }

    /** Attempt the current saved network. */
    void startNextConnection() {
        if (profileIndex_ >= count_) {
            startAccessPoint();
            return;
        }

        WiFi.mode(apActive_ ? WIFI_AP_STA : WIFI_STA);

        const Profile& profile = profiles_[profileIndex_];

        Serial.print("Connecting to ");
        Serial.println(profile.ssid);

        WiFi.begin(
            profile.ssid.c_str(),
            profile.password.c_str()
        );

        connecting_ = true;
        attemptStarted_ = millis();
    }

    /** Start a password-protected setup network. */
    void startAccessPoint() {
        if (apActive_) {
            return;
        }

        if (strlen(HIDLAB_SETUP_PASSWORD) < 8) {
            Serial.println("Invalid setup AP password.");
            return;
        }

        WiFi.mode(WIFI_AP_STA);

        apActive_ = WiFi.softAP(
            "HIDLab-Setup",
            HIDLAB_SETUP_PASSWORD
        );

        if (apActive_) {
            Serial.print("Setup network: http://");
            Serial.println(WiFi.softAPIP());
        }
    }

    /** Restart network discovery following a profile change. */
    void restartConnections() {
        WiFi.disconnect();
        wasConnected_ = false;
        connecting_ = false;
        profileIndex_ = 0;
        attempts_ = 0;
        MDNS.end();

        startNextConnection();
    }
};