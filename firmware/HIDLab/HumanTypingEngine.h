
#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <USBHIDKeyboard.h>
#include <esp_system.h>

#include "PassageQueue.h"

/**
 * Nonblocking USB keyboard playback with configurable typing profiles.
 *
 * Standard mode uses a fixed interval. Humanized mode adjusts its
 * target WPM across groups of words and supports typo corrections.
 */
class HumanTypingEngine {
public:
    enum class Mode {
        Standard,
        Humanized
    };

    void begin() {
        preferences_.begin("hidlab-type", false);

        mode_ = preferences_.getBool("human", false)
            ? Mode::Humanized
            : Mode::Standard;

        // Existing installations can migrate from the old single WPM.
        const int oldWpm = constrain(
            preferences_.getInt("wpm", 35),
            15,
            100
        );

        minWpm_ = constrain(
            preferences_.getInt("minwpm", oldWpm == 35 ? 30 : oldWpm),
            15,
            100
        );

        maxWpm_ = constrain(
            preferences_.getInt("maxwpm", oldWpm),
            15,
            100
        );

        if (minWpm_ > maxWpm_) {
            minWpm_ = maxWpm_;
        }

        typoRate_ = constrain(
            preferences_.getInt("typo", 3),
            0,
            10
        );

        randomSeed(esp_random());
        reset();
    }

    /**
     * Save a validated typing profile to nonvolatile storage.
     */
    bool configure(
        const String& mode,
        int minWpm,
        int maxWpm,
        int typoRate
    ) {
        if (mode != "standard" && mode != "humanized") {
            return false;
        }

        if (minWpm < 15 || minWpm > 100 ||
            maxWpm < 15 || maxWpm > 100 ||
            minWpm > maxWpm ||
            typoRate < 0 || typoRate > 10) {
            return false;
        }

        mode_ = mode == "humanized"
            ? Mode::Humanized
            : Mode::Standard;

        minWpm_ = minWpm;
        maxWpm_ = maxWpm;
        typoRate_ = typoRate;

        preferences_.putBool(
            "human",
            mode_ == Mode::Humanized
        );

        preferences_.putInt("minwpm", minWpm_);
        preferences_.putInt("maxwpm", maxWpm_);
        preferences_.putInt("typo", typoRate_);

        reset();
        return true;
    }

    const char* modeName() const {
        return mode_ == Mode::Humanized
            ? "humanized"
            : "standard";
    }

    int minWpm() const {
        return minWpm_;
    }

    int maxWpm() const {
        return maxWpm_;
    }

    int typoRate() const {
        return typoRate_;
    }

    int currentWpm() const {
        return targetWpm_;
    }

    /**
     * Initialize pacing for a new passage.
     */
    void beginPassage() {
        phase_ = Phase::Ready;

        targetWpm_ = random(
            minWpm_,
            maxWpm_ + 1
        );

        wordsUntilChange_ = random(3, 8);
        dueAt_ = millis() + 250;
    }

    /**
     * Resume an interrupted passage without resetting corrections.
     */
    void resume() {
        dueAt_ = millis() + 250;
    }

    /**
     * Return whether an unintended character still needs erasing.
     */
    bool hasUncorrectedTypo() const {
        return phase_ == Phase::EraseTypo;
    }

    /**
     * Clear the playback engine's temporary state.
     */
    void reset() {
        phase_ = Phase::Ready;
        targetWpm_ = minWpm_;
        wordsUntilChange_ = 0;
        dueAt_ = 0;
    }

    /**
     * Emit no more than one keyboard operation per update.
     *
     * Passage position advances only after an accepted correct key.
     */
    void update(
        PassageQueue& queue,
        USBHIDKeyboard& keyboard,
        bool usbReady
    ) {
        if (!usbReady ||
            queue.state() != PassageQueue::State::Typing) {
            return;
        }

        const unsigned long now = millis();

        if (static_cast<int32_t>(now - dueAt_) < 0) {
            return;
        }

        char expected;

        if (!queue.peek(expected)) {
            return;
        }

        if (phase_ == Phase::EraseTypo) {
            if (keyboard.write(KEY_BACKSPACE) == 1) {
                phase_ = Phase::TypeCorrect;
                dueAt_ = now + random(90, 210);
            }
            return;
        }

        if (phase_ == Phase::TypeCorrect) {
            if (keyboard.write(
                static_cast<uint8_t>(expected)
            ) == 1) {
                phase_ = Phase::Ready;
                queue.advance();
                scheduleNext(now, expected);
            }
            return;
        }

        char incorrect;

        if (mode_ == Mode::Humanized &&
            typoRate_ > 0 &&
            random(100) < typoRate_ &&
            nearbyLetter(expected, incorrect)) {

            if (keyboard.write(
                static_cast<uint8_t>(incorrect)
            ) == 1) {
                phase_ = Phase::EraseTypo;
                dueAt_ = now + random(100, 260);
            }

            return;
        }

        if (keyboard.write(
            static_cast<uint8_t>(expected)
        ) == 1) {
            queue.advance();
            scheduleNext(now, expected);
        }
    }

private:
    enum class Phase {
        Ready,
        EraseTypo,
        TypeCorrect
    };

    Preferences preferences_;

    Mode mode_ = Mode::Standard;
    Phase phase_ = Phase::Ready;

    int minWpm_ = 30;
    int maxWpm_ = 35;
    int typoRate_ = 3;

    int targetWpm_ = 30;
    int wordsUntilChange_ = 0;

    unsigned long dueAt_ = 0;

    /**
     * Choose an adjacent letter on an English QWERTY keyboard.
     */
    static bool nearbyLetter(char input, char& result) {
        const bool uppercase =
            input >= 'A' && input <= 'Z';

        const char lower = uppercase
            ? input - 'A' + 'a'
            : input;

        const char* rows[] = {
            "qwertyuiop",
            "asdfghjkl",
            "zxcvbnm"
        };

        for (const char* row : rows) {
            for (int i = 0; row[i] != '\0'; ++i) {
                if (row[i] != lower) {
                    continue;
                }

                const bool hasLeft = i > 0;
                const bool hasRight = row[i + 1] != '\0';

                int offset;

                if (hasLeft && hasRight) {
                    offset = random(2) == 0 ? -1 : 1;
                } else {
                    offset = hasLeft ? -1 : 1;
                }

                result = row[i + offset];

                if (uppercase) {
                    result = result - 'a' + 'A';
                }

                return true;
            }
        }

        return false;
    }

    /**
     * Change the target WPM gradually every few words.
     */
    void updateTargetSpeed(char typed) {
        if (mode_ != Mode::Humanized) {
            return;
        }

        if (typed != ' ' && typed != '\n') {
            return;
        }

        --wordsUntilChange_;

        if (wordsUntilChange_ > 0) {
            return;
        }

        wordsUntilChange_ = random(3, 8);

        if (minWpm_ == maxWpm_) {
            targetWpm_ = minWpm_;
            return;
        }

        // Small changes produce a gradual speed transition.
        const int change = random(-2, 3);

        targetWpm_ = constrain(
            targetWpm_ + change,
            minWpm_,
            maxWpm_
        );
    }

    /**
     * Schedule the next key according to the current typing mode.
     */
    void scheduleNext(unsigned long now, char typed) {
        if (mode_ == Mode::Standard) {
            dueAt_ = now + 150;
            return;
        }

        updateTargetSpeed(typed);

        // Approximate WPM uses five characters per word.
        const unsigned long base =
            12000UL / static_cast<unsigned long>(targetWpm_);

        unsigned long delayMs =
            base * static_cast<unsigned long>(
                random(80, 121)
            ) / 100UL;

        if (typed == '.' ||
            typed == '!' ||
            typed == '?' ||
            typed == '\n') {

            delayMs += random(220, 550);

        } else if (typed == ',' ||
                   typed == ';' ||
                   typed == ':') {

            delayMs += random(90, 250);

        } else if (typed == ' ') {
            delayMs += random(15, 80);
        }

        if (random(100) < 3) {
            delayMs += random(180, 600);
        }

        dueAt_ = now + delayMs;
    }
};
