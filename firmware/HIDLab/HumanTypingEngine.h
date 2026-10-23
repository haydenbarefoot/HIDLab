
#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <USBHIDKeyboard.h>
#include <esp_system.h>
#include "PassageQueue.h"

/** Nonblocking standard and varied-cadence USB keyboard playback. */
class HumanTypingEngine {
public:
    enum class Mode { Standard, Humanized };

    void begin() {
        preferences_.begin("hidlab-type", false);
        mode_ = preferences_.getBool("human", false)
            ? Mode::Humanized : Mode::Standard;
        wpm_ = constrain(preferences_.getInt("wpm", 35), 15, 100);
        typoRate_ = constrain(preferences_.getInt("typo", 3), 0, 10);
        randomSeed(esp_random());
    }

    bool configure(const String& mode, int wpm, int typoRate) {
        if ((mode != "standard" && mode != "humanized") ||
            wpm < 15 || wpm > 100 || typoRate < 0 || typoRate > 10) {
            return false;
        }

        mode_ = mode == "humanized" ? Mode::Humanized : Mode::Standard;
        wpm_ = wpm;
        typoRate_ = typoRate;
        preferences_.putBool("human", mode_ == Mode::Humanized);
        preferences_.putInt("wpm", wpm_);
        preferences_.putInt("typo", typoRate_);
        reset();
        return true;
    }

    const char* modeName() const {
        return mode_ == Mode::Humanized ? "humanized" : "standard";
    }

    int wpm() const { return wpm_; }
    int typoRate() const { return typoRate_; }

    /** Begin or resume playback without discarding a pending correction. */
    void beginPassage() {
        phase_ = Phase::Ready;
        dueAt_ = millis() + 250;
    }

    void resume() {
        dueAt_ = millis() + 250;
    }

    /** A correction is pending if an unintended character is visible. */
    bool hasUncorrectedTypo() const {
        return phase_ == Phase::EraseTypo;
    }

    void reset() {
        phase_ = Phase::Ready;
        dueAt_ = 0;
    }

    /** Emit at most one keyboard event and never block the web server. */
    void update(PassageQueue& queue, USBHIDKeyboard& keyboard, bool usbReady) {
        if (!usbReady || queue.state() != PassageQueue::State::Typing) {
            return;
        }

        const unsigned long now = millis();
        if (static_cast<int32_t>(now - dueAt_) < 0) return;

        char expected;
        if (!queue.peek(expected)) return;

        if (phase_ == Phase::EraseTypo) {
            if (keyboard.write(KEY_BACKSPACE) == 1) {
                phase_ = Phase::TypeCorrect;
                dueAt_ = now + random(90, 210);
            }
            return;
        }

        if (phase_ == Phase::TypeCorrect) {
            if (keyboard.write(static_cast<uint8_t>(expected)) == 1) {
                phase_ = Phase::Ready;
                queue.advance();
                scheduleNext(now, expected);
            }
            return;
        }

        char wrong;
        if (mode_ == Mode::Humanized && typoRate_ > 0 &&
            random(100) < typoRate_ && nearbyLetter(expected, wrong)) {
            if (keyboard.write(static_cast<uint8_t>(wrong)) == 1) {
                phase_ = Phase::EraseTypo;
                dueAt_ = now + random(100, 260);
            }
            return;
        }

        if (keyboard.write(static_cast<uint8_t>(expected)) == 1) {
            queue.advance();
            scheduleNext(now, expected);
        }
    }

private:
    enum class Phase { Ready, EraseTypo, TypeCorrect };

    Preferences preferences_;
    Mode mode_ = Mode::Standard;
    Phase phase_ = Phase::Ready;
    int wpm_ = 35;
    int typoRate_ = 3;
    unsigned long dueAt_ = 0;

    /** Choose a nearby letter on a conventional US QWERTY keyboard. */
    static bool nearbyLetter(char input, char& result) {
        const bool upper = input >= 'A' && input <= 'Z';
        const char lower = upper ? input - 'A' + 'a' : input;
        const char* rows[] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};

        for (const char* row : rows) {
            for (int i = 0; row[i] != '\0'; ++i) {
                if (row[i] != lower) continue;

                const bool left = i > 0;
                const bool right = row[i + 1] != '\0';
                const int offset = left && right
                    ? (random(2) == 0 ? -1 : 1)
                    : (left ? -1 : 1);

                result = row[i + offset];
                if (upper) result = result - 'a' + 'A';
                return true;
            }
        }
        return false;
    }

    /** Schedule the next key event from the configured typing profile. */
    void scheduleNext(unsigned long now, char typed) {
        if (mode_ == Mode::Standard) {
            dueAt_ = now + 150;
            return;
        }

        // Five characters per word is the conventional WPM approximation.
        const unsigned long base = 12000UL / static_cast<unsigned long>(wpm_);
        unsigned long delayMs = base * static_cast<unsigned long>(random(70, 141)) / 100UL;

        if (typed == '.' || typed == '!' || typed == '?' || typed == '\n') {
            delayMs += random(250, 650);
        } else if (typed == ',' || typed == ';' || typed == ':') {
            delayMs += random(100, 280);
        } else if (typed == ' ') {
            delayMs += random(15, 90);
        }

        // Occasional short hesitations for a less mechanically even cadence.
        if (random(100) < 3) delayMs += random(200, 750);

        dueAt_ = now + delayMs;
    }
};
