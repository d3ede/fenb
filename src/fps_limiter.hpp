#pragma once
#include "common.hpp"

namespace fps_detail {
struct Binding {
    volatile LONG *enabled = nullptr;
    DWORD *lastTick = nullptr;
    LONG *elapsed = nullptr;
    explicit operator bool() const {
        return enabled && lastTick && elapsed;
    }
};
// The caller owns the mapped image. No scanning or writes on unrecognized images.
Binding InspectImage(HMODULE image);
bool KnownClientFile(const wchar_t *path);

struct State {
    explicit State(Binding binding = {}) : binding(binding) {}
    Binding binding;
    std::mutex mutex;
    unsigned owners = 0;
    LONG original = 0;
};
} // namespace fps_detail

class FpsLimiter {
  public:
    FpsLimiter() = default;
    explicit FpsLimiter(fps_detail::State &state) : state_(&state) {}
    ~FpsLimiter();
    FpsLimiter(const FpsLimiter &) = delete;
    FpsLimiter &operator=(const FpsLimiter &) = delete;
    void update(bool unlock);
    bool supported();
    bool active() const {
        return active_;
    }

  private:
    fps_detail::State &state();
    fps_detail::State *state_ = nullptr;
    bool active_ = false;
};
