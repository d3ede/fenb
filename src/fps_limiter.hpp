#pragma once
#include "common.hpp"
#include "fixed_step.hpp"

namespace fps_detail {
using FrameMove = HRESULT(__thiscall *)(void *);
int64_t ReadCounter();
struct Binding {
    volatile LONG *enabled = nullptr;
    DWORD *lastTick = nullptr;
    LONG *elapsed = nullptr;
    void *application = nullptr;
    FrameMove *frameMove = nullptr;
    FrameMove originalMove = nullptr;
    float *frameElapsed = nullptr;
    explicit operator bool() const {
        return enabled && lastTick && elapsed && application && frameMove && originalMove && frameElapsed;
    }
};
// The caller owns the mapped image. No scanning or writes on unrecognized images.
Binding InspectImage(HMODULE image);
bool KnownClientFile(const wchar_t *path);

struct State {
    explicit State(Binding binding = {});
    Binding binding;
    FixedStep scheduler;
    int64_t (*counter)() = ReadCounter;
    int64_t frequency = 0;
    unsigned owners = 0;
    DWORD thread = 0;
    LONG originalFlag = 0;
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
