#pragma once

// Restores the 64-bit view after WOW64 file-system redirection.

class ScopedWow64RedirectionDisabled {
public:
    ScopedWow64RedirectionDisabled()
        : valid_(Wow64DisableWow64FsRedirection(&previous_) != FALSE) {
    }

    ~ScopedWow64RedirectionDisabled() {
        if (valid_) {
            Wow64RevertWow64FsRedirection(previous_);
        }
    }

    ScopedWow64RedirectionDisabled(const ScopedWow64RedirectionDisabled&) = delete;
    ScopedWow64RedirectionDisabled& operator=(
        const ScopedWow64RedirectionDisabled&) = delete;

    bool valid() const { return valid_; }

private:
    bool valid_ = false;
    void* previous_ = nullptr;
};
