
#pragma once

#include <Arduino.h>

class PassageQueue {
public:
    static constexpr size_t MAX_PASSAGES = 2;
    static constexpr size_t MAX_CHARACTERS = 12000;

    enum class State {
        Idle,
        Typing,
        Paused
    };

    /**
     * Add a passage to the end of the waiting queue.
     * Returns false if the passage is invalid or capacity is reached.
     */
    bool add(const String& text) {
        if (!validText(text) || totalCount() >= MAX_PASSAGES) {
            return false;
        }

        passages_[count_++] = text;
        return true;
    }

    /** Remove a waiting passage by its zero-based index. */
    bool remove(size_t index) {
        if (index >= count_) {
            return false;
        }

        for (size_t i = index; i + 1 < count_; ++i) {
            passages_[i] = passages_[i + 1];
        }

        passages_[--count_] = "";
        return true;
    }

    /** Replace the contents of a waiting passage. */
    bool edit(size_t index, const String& text) {
        if (index >= count_ || !validText(text)) {
            return false;
        }

        passages_[index] = text;
        return true;
    }

    /**
     * Move a waiting passage one position.
     * Direction must be -1 (up) or 1 (down).
     */
    bool move(size_t index, int direction) {
        if (index >= count_ || (direction != -1 && direction != 1)) {
            return false;
        }

        int destination = static_cast<int>(index) + direction;

        if (destination < 0 ||
            destination >= static_cast<int>(count_)) {
            return false;
        }

        String temporary = passages_[index];
        passages_[index] = passages_[destination];
        passages_[destination] = temporary;

        return true;
    }

    /** Start the first waiting passage. */
    bool start() {
        if (state_ != State::Idle || count_ == 0) {
            return false;
        }

        activeText_ = passages_[0];
        active_ = true;
        position_ = 0;

        remove(0);
        state_ = State::Typing;

        return true;
    }

    /** Suspend an active passage without resetting its position. */
    bool pause() {
        if (state_ != State::Typing) {
            return false;
        }

        state_ = State::Paused;
        return true;
    }

    /** Continue processing a paused passage. */
    bool resume() {
        if (state_ != State::Paused) {
            return false;
        }

        state_ = State::Typing;
        return true;
    }

    /** Return the active passage to the front of the waiting queue. */
    bool stop() {
        if (state_ == State::Idle || !active_) {
            return false;
        }

        // The active passage already counts toward the capacity limit.
        for (size_t i = count_; i > 0; --i) {
            passages_[i] = passages_[i - 1];
        }

        passages_[0] = activeText_;
        ++count_;

        clearActive();
        return true;
    }

    /**
     * Read and advance one character while typing.
     * Returns false when there is no character available.
     *
     * This will later be called by the USB HID output task.
     */
    bool nextCharacter(char& output) {
        if (state_ != State::Typing || !active_) {
            return false;
        }

        if (position_ >= activeText_.length()) {
            clearActive();
            return false;
        }

        output = activeText_[position_++];

        if (position_ >= activeText_.length()) {
            clearActive();
        }

        return true;
    }

    size_t waitingCount() const {
        return count_;
    }

    size_t totalCount() const {
        return count_ + (active_ ? 1 : 0);
    }

    size_t position() const {
        return position_;
    }

    State state() const {
        return state_;
    }

    const String& passage(size_t index) const {
        static const String empty;
        return index < count_ ? passages_[index] : empty;
    }

    const char* statusName() const {
        switch (state_) {
            case State::Typing:
                return "typing";

            case State::Paused:
                return "paused";

            default:
                return "idle";
        }
    }

private:
    String passages_[MAX_PASSAGES];
    size_t count_ = 0;

    String activeText_;
    size_t position_ = 0;
    bool active_ = false;
    State state_ = State::Idle;

    /** Ensure passages contain non-whitespace text within the size limit. */
    static bool validText(const String& text) {
        if (text.isEmpty() || text.length() > MAX_CHARACTERS) {
            return false;
        }

        for (size_t i = 0; i < text.length(); ++i) {
            if (!isspace(static_cast<unsigned char>(text[i]))) {
                return true;
            }
        }

        return false;
    }

    /** Discard a completed or stopped passage and reset playback state. */
    void clearActive() {
        activeText_ = "";
        active_ = false;
        position_ = 0;
        state_ = State::Idle;
    }
};
