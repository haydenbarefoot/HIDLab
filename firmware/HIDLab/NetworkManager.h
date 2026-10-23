
#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include "wifi_credentials.h"

#ifndef HIDLAB_SETUP_PASSWORD
#error Define HIDLAB_SETUP_PASSWORD in wifi_credentials.h
#endif

class NetworkManager {
public:
    static constexpr int MAX_NETWORKS = 4;
    static constexpr unsigned long CONNECT_TIMEOUT_MS = 9000;

    void begin() {
        if (!prefs_.begin("hidlab-wifi", false)) {
            Serial.println("Wi-Fi storage unavailable");
        }

        load();

        if (!prefs_.getBool("initialized", false)) {
            prefs_.putBool("initialized", true);

            if (count_ == 0 && strlen(WIFI_SSID) > 0) {
                profiles_[0] = {
                    String(WIFI_SSID),
                    String(WIFI_PASSWORD)
                };

                count_ = 1;
                persist();
            }
        }

        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(false);
        startCycle(-1);
    }

    /** Progress connection attempts and manage fallback setup Wi-Fi. */
    void update() {
        if (reconfigureAt_ != 0 &&
            static_cast<int32_t>(millis() - reconfigureAt_) >= 0) {

            reconfigureAt_ = 0;
            WiFi.disconnect();
            connectedIndex_ = -1;
            wasConnected_ = false;
            stopMdns();
            startCycle(-1);
        }

        if (WiFi.status() == WL_CONNECTED) {
            if (!wasConnected_) {
                wasConnected_ = true;
                ++generation_;
                everConnected_ = true;
                trying_ = false;
                exhausted_ = false;

                connectedIndex_ = orderPos_ < orderCount_
                    ? order_[orderPos_]
                    : -1;

                Serial.print("HIDLab IP: http://");
                Serial.println(WiFi.localIP());

                if (MDNS.begin("hidlab")) {
                    mdnsRunning_ = true;
                    MDNS.addService("http", "tcp", 80);
                }

                if (apActive_) {
                    WiFi.softAPdisconnect(false);
                    apActive_ = false;
                    WiFi.mode(WIFI_STA);
                }
            }

            return;
        }

        if (wasConnected_) {
            wasConnected_ = false;
            ++generation_;
            stopMdns();
            startCycle(connectedIndex_);
        }

        if (exhausted_ || !trying_) return;

        if (millis() - attemptAt_ < CONNECT_TIMEOUT_MS) {
            return;
        }

        WiFi.disconnect();
        ++attemptsOnProfile_;

        const int maxAttempts = orderPos_ == 0 ? 3 : 1;

        if (attemptsOnProfile_ >= maxAttempts) {
            ++orderPos_;
            attemptsOnProfile_ = 0;
        }

        attemptNext();
    }

    /** Add or update a saved Wi-Fi profile. */
    bool addOrUpdate(const String& ssid, const String& password) {
        if (ssid.isEmpty() ||
            ssid.length() > 32 ||
            !(password.isEmpty() ||
              (password.length() >= 8 && password.length() <= 63))) {
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
            if (count_ >= MAX_NETWORKS) return false;
            index = count_++;
        }

        profiles_[index] = {ssid, password};
        persist();

        // Wait for the HTTP response before changing connections.
        if (!connected() || connectedIndex_ == index) {
            scheduleReconnect();
        }

        return true;
    }

    bool remove(int index) {
        if (index < 0 || index >= count_) return false;

        const bool removingCurrent = index == connectedIndex_;

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
        return connected() ? WiFi.SSID() : String();
    }

    String address() const {
        if (connected()) return WiFi.localIP().toString();
        if (apActive_) return WiFi.softAPIP().toString();
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
    bool everConnected_ = false;
    bool exhausted_ = false;
    bool apActive_ = false;
    bool mdnsRunning_ = false;

    unsigned long attemptAt_ = 0;
    unsigned long reconfigureAt_ = 0;

    void load() {
        count_ = constrain(
            prefs_.getInt("count", 0),
            0,
            MAX_NETWORKS
        );

        for (int i = 0; i < count_; ++i) {
            const String key = "wifi" + String(i);

            profiles_[i].ssid =
                prefs_.getString((key + "s").c_str(), "");

            profiles_[i].password =
                prefs_.getString((key + "p").c_str(), "");
        }
    }

    void persist() {
        prefs_.putInt("count", count_);

        for (int i = 0; i < MAX_NETWORKS; ++i) {
            const String key = "wifi" + String(i);

            prefs_.putString(
                (key + "s").c_str(),
                i < count_ ? profiles_[i].ssid : String()
            );

            prefs_.putString(
                (key + "p").c_str(),
                i < count_ ? profiles_[i].password : String()
            );
        }
    }

    void stopMdns() {
        if (mdnsRunning_) MDNS.end();
        mdnsRunning_ = false;
    }

    void scheduleReconnect() {
        reconfigureAt_ = millis() + 1500;
    }

    /** Prefer a visible saved network or the last connected one. */
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

        if (preferredIndex < 0 || preferredIndex >= count_) {
            const int found = WiFi.scanNetworks(false, false);
            int strongest = -999;

            for (int scan = 0; scan < found; ++scan) {
                for (int i = 0; i < count_; ++i) {
                    if (WiFi.SSID(scan) == profiles_[i].ssid &&
                        WiFi.RSSI(scan) > strongest) {

                        preferredIndex = i;
                        strongest = WiFi.RSSI(scan);
                    }
                }
            }

            WiFi.scanDelete();

            if (preferredIndex < 0) {
                preferredIndex = 0;
            }
        }

        order_[orderCount_++] = preferredIndex;

        for (int i = 0; i < count_; ++i) {
            if (i != preferredIndex) {
                order_[orderCount_++] = i;
            }
        }

        attemptNext();
    }

    void attemptNext() {
        if (orderPos_ >= orderCount_) {
            finishCycle();
            return;
        }

        const Profile& profile = profiles_[order_[orderPos_]];

        WiFi.mode(apActive_ ? WIFI_AP_STA : WIFI_STA);

        WiFi.begin(
            profile.ssid.c_str(),
            profile.password.c_str()
        );

        trying_ = true;
        attemptAt_ = millis();

        Serial.print("Trying Wi-Fi: ");
        Serial.println(profile.ssid);
    }

    void finishCycle() {
        trying_ = false;
        exhausted_ = true;

        // After losing a working connection, wait for device restart.
        if (!everConnected_) {
            startAccessPoint();
        }

        Serial.println("Saved Wi-Fi attempts exhausted");
    }

    void startAccessPoint() {
        if (apActive_) return;

        if (strlen(HIDLAB_SETUP_PASSWORD) < 8) {
            Serial.println("Setup password must be at least 8 characters");
            return;
        }

        WiFi.mode(WIFI_AP_STA);

        apActive_ = WiFi.softAP(
            "HIDLab-Setup",
            HIDLAB_SETUP_PASSWORD
        );

        if (apActive_) {
            ++generation_;

            Serial.print("Setup AP: http://");
            Serial.println(WiFi.softAPIP());
        }
    }
};
