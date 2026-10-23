
#pragma once

#include <Arduino.h>
#include <ctype.h>

class PassageQueue {
public:
    static constexpr size_t MAX_PASSAGES = 2;
    static constexpr size_t MAX_CHARACTERS = 12000;

    enum class State { Idle, Typing, Paused };

    static bool validText(const String& text) {
        if (text.isEmpty() || text.length() > MAX_CHARACTERS) return false;

        bool nonWhitespace = false;

        for (size_t i = 0; i < text.length(); ++i) {
            const unsigned char c = static_cast<unsigned char>(text[i]);

            if (!((c >= 32 && c <= 126) || c == '\n' || c == '\r')) {
                return false;
            }

            if (c != ' ' && c != '\n' && c != '\r') {
                nonWhitespace = true;
            }
        }

        return nonWhitespace;
    }

    bool add(const String& text) {
        if (!validText(text) || totalCount() >= MAX_PASSAGES) {
            return false;
        }

        waiting_[count_++] = normalize(text);
        return true;
    }

    bool edit(size_t index, const String& text) {
        if (index >= count_ || !validText(text)) {
            return false;
        }

        waiting_[index] = normalize(text);
        return true;
    }

    bool remove(size_t index) {
        if (index >= count_) return false;

        for (size_t i = index; i + 1 < count_; ++i) {
            waiting_[i] = waiting_[i + 1];
        }

        waiting_[--count_] = "";
        return true;
    }

    bool move(size_t index, int direction) {
        if (index >= count_ || (direction != -1 && direction != 1)) {
            return false;
        }

        const int target = static_cast<int>(index) + direction;

        if (target < 0 || target >= static_cast<int>(count_)) {
            return false;
        }

        String temp = waiting_[index];
        waiting_[index] = waiting_[target];
        waiting_[target] = temp;
        return true;
    }

    bool start() {
        if (state_ != State::Idle || count_ == 0) return false;

        active_ = waiting_[0];
        hasActive_ = true;
        position_ = 0;

        remove(0);
        state_ = State::Typing;
        return true;
    }

    bool pause() {
        if (state_ != State::Typing) return false;
        state_ = State::Paused;
        return true;
    }

    bool resume() {
        if (state_ != State::Paused) return false;
        state_ = State::Typing;
        return true;
    }

    bool stop() {
        if (!hasActive_ || state_ == State::Idle) return false;

        // Restore the original passage ahead of other waiting items.
        for (size_t i = count_; i > 0; --i) {
            waiting_[i] = waiting_[i - 1];
        }

        waiting_[0] = active_;
        ++count_;
        clearActive();
        return true;
    }

    /** Read the current character without advancing its position. */
    bool peek(char& out) const {
        if (!hasActive_ ||
            state_ != State::Typing ||
            position_ >= active_.length()) {
            return false;
        }

        out = active_[position_];
        return true;
    }

    /** Advance after the keyboard backend accepts a character. */
    void advance() {
        if (state_ != State::Typing || !hasActive_) return;

        if (++position_ >= active_.length()) {
            clearActive();
        }
    }

    size_t waitingCount() const {
        return count_;
    }

    size_t totalCount() const {
        return count_ + static_cast<size_t>(hasActive_);
    }

    size_t position() const {
        return position_;
    }

    State state() const {
        return state_;
    }

    const String& passage(size_t index) const {
        static const String empty;
        return index < count_ ? waiting_[index] : empty;
    }

    const char* statusName() const {
        return state_ == State::Typing ? "typing" :
               state_ == State::Paused ? "paused" : "idle";
    }

private:
    static String normalize(const String& text) {
        String result = text;
        result.replace("\r\n", "\n");
        result.replace("\r", "\n");
        return result;
    }

    void clearActive() {
        active_ = "";
        hasActive_ = false;
        position_ = 0;
        state_ = State::Idle;
    }

    String waiting_[MAX_PASSAGES];
    String active_;
    size_t count_ = 0;
    size_t position_ = 0;
    bool hasActive_ = false;
    State state_ = State::Idle;
};
