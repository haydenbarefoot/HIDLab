
#pragma once

#ifndef HIDLAB_STARTUP_DIAGNOSTICS
#define HIDLAB_STARTUP_DIAGNOSTICS 0
#endif

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <ESPmDNS.h>

#include "wifi_credentials.h"

#ifndef HIDLAB_SETUP_PASSWORD
#error Define HIDLAB_SETUP_PASSWORD in wifi_credentials.h
#endif

/**
 * Manages HIDLab Wi-Fi profiles and fallback setup access.
 *
 * The network defined in wifi_credentials.h is the primary
 * network and is synchronized on every firmware startup.
 */
class NetworkManager {
public:
    static constexpr int MAX_NETWORKS = 4;
    static constexpr unsigned long CONNECT_TIMEOUT_MS = 9000;

    void begin() {
        if (!prefs_.begin("hidlab-wifi", false)) {
            Serial.println("Wi-Fi preferences unavailable.");
        }

        load();

        // Keep the compiled firmware credentials synchronized
        // with persistent storage on every startup.
        syncFirmwareCredentials();

        // During the temporary diagnostic build, start the AP *before*
        // attempting any saved networks, so the board is inspectable.
#if HIDLAB_STARTUP_DIAGNOSTICS
        WiFi.mode(WIFI_AP_STA);
        WiFi.setAutoReconnect(false);
        startAccessPoint();
#else
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(false);
#endif

        // Always try the firmware-configured network first.
        startCycle(0);
    }

    /**
     * Maintain Wi-Fi connectivity and the configured retry policy.
     */
    void update() {
        if (reconfigureAt_ != 0 &&
            static_cast<int32_t>(
                millis() - reconfigureAt_
            ) >= 0) {

            reconfigureAt_ = 0;

            WiFi.disconnect();

            connectedIndex_ = -1;
            wasConnected_ = false;

            stopMdns();
            startCycle(0);
        }

        if (WiFi.status() == WL_CONNECTED) {
            if (!wasConnected_) {
                wasConnected_ = true;
                ++generation_;

                trying_ = false;
                exhausted_ = false;

                connectedIndex_ =
                    orderPos_ < orderCount_
                    ? order_[orderPos_]
                    : -1;

                Serial.print("Connected to: ");
                Serial.println(WiFi.SSID());

                Serial.print("HIDLab IP: http://");
                Serial.println(WiFi.localIP());

                if (!mdnsRunning_ && MDNS.begin("hidlab")) {
                    mdnsRunning_ = true;
                    MDNS.addService("http", "tcp", 80);
                }

                // Keep diagnostic AP available even when STA connects.
                // Normal builds retain the original fallback-only design.
#if !HIDLAB_STARTUP_DIAGNOSTICS
                if (apActive_) {
                    WiFi.softAPdisconnect(false);
                    apActive_ = false;
                    WiFi.mode(WIFI_STA);
                }
#endif
            }

            return;
        }

        if (wasConnected_) {
            wasConnected_ = false;
            ++generation_;

            stopMdns();

            // Retry the previously connected network first.
            startCycle(connectedIndex_);
        }

        if (exhausted_ || !trying_) {
            return;
        }

        if (millis() - attemptAt_ < CONNECT_TIMEOUT_MS) {
            return;
        }

        WiFi.disconnect();

        ++attemptsOnProfile_;

        // First network gets three attempts; others get one.
        const int maxAttempts =
            orderPos_ == 0 ? 3 : 1;

        if (attemptsOnProfile_ >= maxAttempts) {
            ++orderPos_;
            attemptsOnProfile_ = 0;
        }

        attemptNext();
    }

    /**
     * Add or update a persistent Wi-Fi network.
     */
    bool addOrUpdate(
        const String& ssid,
        const String& password
    ) {
        if (!validCredentials(ssid, password)) {
            return false;
        }

        int index = -1;

        for (int i = 0; i < count_; ++i) {
            if (profiles_[i].ssid == ssid) {
                index = i;
                break;
            }
        }

        if (index < 0) {
            if (count_ >= MAX_NETWORKS) {
                return false;
            }

            index = count_++;
        }

        profiles_[index] = {ssid, password};
        persist();

        if (!connected() || connectedIndex_ == index) {
            scheduleReconnect();
        }

        return true;
    }

    /**
     * Remove a saved network.
     */
    bool remove(int index) {
        if (index < 0 || index >= count_) {
            return false;
        }

        const bool removingCurrent =
            index == connectedIndex_;

        for (int i = index; i < count_ - 1; ++i) {
            profiles_[i] = profiles_[i + 1];
        }

        profiles_[--count_] = {"", ""};

        persist();

        if (removingCurrent || !connected()) {
            scheduleReconnect();
        } else if (connectedIndex_ > index) {
            --connectedIndex_;
        }

        return true;
    }

    uint32_t generation() const {
        return generation_;
    }

    int count() const {
        return count_;
    }

    String name(int index) const {
        return index >= 0 && index < count_
            ? profiles_[index].ssid
            : String();
    }

    bool connected() const {
        return WiFi.status() == WL_CONNECTED;
    }

    bool setupMode() const {
        return apActive_;
    }

    bool exhausted() const {
        return exhausted_;
    }

    String currentSSID() const {
        return connected()
            ? WiFi.SSID()
            : String();
    }

    String address() const {
        if (connected()) {
            return WiFi.localIP().toString();
        }

        if (apActive_) {
            return WiFi.softAPIP().toString();
        }

        return String();
    }

private:
    struct Profile {
        String ssid;
        String password;
    };

    Preferences prefs_;
    Profile profiles_[MAX_NETWORKS];

    int count_ = 0;
    uint32_t generation_ = 0;

    int order_[MAX_NETWORKS] = {};
    int orderCount_ = 0;
    int orderPos_ = 0;
    int attemptsOnProfile_ = 0;
    int connectedIndex_ = -1;

    bool trying_ = false;
    bool wasConnected_ = false;
    bool exhausted_ = false;
    bool apActive_ = false;
    bool mdnsRunning_ = false;

    unsigned long attemptAt_ = 0;
    unsigned long reconfigureAt_ = 0;

    /**
     * Validate WPA password length and SSID length.
     */
    static bool validCredentials(
        const String& ssid,
        const String& password
    ) {
        return !ssid.isEmpty() &&
               ssid.length() <= 32 &&
               (
                   password.isEmpty() ||
                   (password.length() >= 8 &&
                    password.length() <= 63)
               );
    }

    /**
     * Load existing persistent network profiles.
     */
    void load() {
        count_ = constrain(
            prefs_.getInt("count", 0),
            0,
            MAX_NETWORKS
        );

        for (int i = 0; i < count_; ++i) {
            const String key = "wifi" + String(i);

            profiles_[i].ssid = prefs_.getString(
                (key + "s").c_str(),
                ""
            );

            profiles_[i].password = prefs_.getString(
                (key + "p").c_str(),
                ""
            );
        }
    }

    /**
     * Synchronize the configured network on every startup.
     *
     * If the firmware's SSID changed, replace the previous
     * primary network. Other saved profiles remain intact.
     *
     * If the configured SSID is already saved elsewhere,
     * move it into the primary position.
     */
    void syncFirmwareCredentials() {
        const String ssid(WIFI_SSID);
        const String password(WIFI_PASSWORD);

        if (!validCredentials(ssid, password)) {
            Serial.println(
                "No valid firmware Wi-Fi credentials configured."
            );
            return;
        }

        // Same SSID: update its password only if changed.
        if (count_ > 0 && profiles_[0].ssid == ssid) {
            if (profiles_[0].password != password) {
                profiles_[0].password = password;
                persist();

                Serial.println(
                    "Primary Wi-Fi password updated."
                );
            }

            return;
        }

        int existingIndex = -1;

        for (int i = 1; i < count_; ++i) {
            if (profiles_[i].ssid == ssid) {
                existingIndex = i;
                break;
            }
        }

        if (existingIndex >= 0) {
            // Move an already-saved network to first priority.
            for (int i = existingIndex; i > 0; --i) {
                profiles_[i] = profiles_[i - 1];
            }

        } else if (count_ == 0) {
            count_ = 1;
        }

        // Replace the old primary when the SSID changes.
        profiles_[0] = {ssid, password};

        persist();

        Serial.print("Primary Wi-Fi synchronized: ");
        Serial.println(ssid);
    }

    /**
     * Save network profiles only when they change.
     */
    void persist() {
        prefs_.putInt("count", count_);

        for (int i = 0; i < MAX_NETWORKS; ++i) {
            const String key = "wifi" + String(i);

            prefs_.putString(
                (key + "s").c_str(),
                i < count_
                    ? profiles_[i].ssid
                    : String()
            );

            prefs_.putString(
                (key + "p").c_str(),
                i < count_
                    ? profiles_[i].password
                    : String()
            );
        }
    }

    /**
     * Stop the currently registered mDNS service.
     */
    void stopMdns() {
        if (mdnsRunning_) {
            MDNS.end();
        }

        mdnsRunning_ = false;
    }

    /**
     * Delay reconnection until the HTTP response is sent.
     */
    void scheduleReconnect() {
        reconfigureAt_ = millis() + 1500;
    }

    /**
     * Construct the retry order, placing the preferred
     * network first.
     */
    void startCycle(int preferredIndex) {
        exhausted_ = false;
        trying_ = false;

        orderCount_ = 0;
        orderPos_ = 0;
        attemptsOnProfile_ = 0;

        if (count_ == 0) {
            finishCycle();
            return;
        }

        if (preferredIndex < 0 ||
            preferredIndex >= count_) {
            preferredIndex = 0;
        }

        order_[orderCount_++] = preferredIndex;

        for (int i = 0; i < count_; ++i) {
            if (i != preferredIndex) {
                order_[orderCount_++] = i;
            }
        }

        attemptNext();
    }

    /**
     * Connect to the selected saved Wi-Fi profile.
     */
    void attemptNext() {
        if (orderPos_ >= orderCount_) {
            finishCycle();
            return;
        }

        const Profile& profile =
            profiles_[order_[orderPos_]];

        WiFi.mode(
            apActive_ ? WIFI_AP_STA : WIFI_STA
        );

        WiFi.begin(
            profile.ssid.c_str(),
            profile.password.c_str()
        );

        trying_ = true;
        attemptAt_ = millis();

        Serial.print("Trying Wi-Fi: ");
        Serial.println(profile.ssid);
    }

    /**
     * Enter fallback setup mode if initial Wi-Fi
     * connection attempts fail.
     */
    void finishCycle() {
        trying_ = false;
        exhausted_ = true;

        if (!apActive_) {
            startAccessPoint();
        }

        Serial.println(
            "Saved Wi-Fi connection attempts exhausted."
        );
    }

    /**
     * Start the password-protected setup network.
     */
    void startAccessPoint() {
        if (apActive_) {
            return;
        }

        // Use the user's existing private setup password for diagnostics.
        const char* apPassword = HIDLAB_SETUP_PASSWORD;

        if (strlen(apPassword) < 8) {
            Serial.println(
                "Setup password must contain at least 8 characters."
            );
            return;
        }

        WiFi.mode(WIFI_AP_STA);

        apActive_ = WiFi.softAP(
            "HIDLab-Setup",
            apPassword
        );

        if (apActive_) {
            ++generation_;

            Serial.print("Setup AP: http://");
            Serial.println(WiFi.softAPIP());
        } else {
            Serial.println(
                "Failed to start HIDLab setup AP."
            );
        }
    }
};
