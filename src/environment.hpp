#pragma once
#include "settings.hpp"
#include "client_layout.hpp"
#include <array>

namespace environment_detail {
// Structure fields require the verified Homesickness x86 renderer ABI.
struct Clock {
    double current, begin;
    int fixed, second, minute, hour, day;
};
struct Timeline {
    double seconds = 43200;
    float selected = -1;
    ULONGLONG tick = 0;
    bool frozen = true;
    double sample(float minutes, bool freeze, ULONGLONG now);
    void reset();
};
using Render = HRESULT(__thiscall *)(void *);
using InitFall = void(__thiscall *)(void *);
struct Binding {
    void *application = nullptr;
    Render *entry = nullptr;
    Render render = nullptr;
    Clock *clock = nullptr;
    unsigned char *sky = nullptr;
    int *weatherOption = nullptr;
    void **player = nullptr;
    InitFall initFall = nullptr;
    explicit operator bool() const {
        return application && entry && render && clock && sky && weatherOption && player && initFall;
    }
};
Binding InspectImage(HMODULE image, const client_detail::Layout &layout = {});
// Native fall geometry has a fixed 2000-particle capacity; the supported client uses at most 400.
constexpr size_t ParticleBytes = 2000 * 3 * sizeof(float) * 2;
struct FrameState {
    Timeline time;
    alignas(float) std::array<unsigned char, ParticleBytes> particles{}, savedParticles{};
    Com<IDirect3DTexture9> texture;
    void *player = nullptr;
    int mode = -1, capacity = 0;
    ULONGLONG weatherTick = 0;
    void reset();
};
class FrameScope {
  public:
    FrameScope(const Binding &, FrameState &, const Settings &, ULONGLONG now);
    ~FrameScope();
    FrameScope(const FrameScope &) = delete;
    FrameScope &operator=(const FrameScope &) = delete;

  private:
    void restoreWeather();
    Binding b_;
    FrameState &state_;
    std::array<int, 5> clock_{};
    bool time_ = false, weather_ = false;
    int mode_ = 0, count_ = 0, option_ = 0;
    IDirect3DTexture9 *texture_ = nullptr;
};
} // namespace environment_detail

class Environment {
  public:
    ~Environment();
    bool supported();
    void update(const Settings &, bool compare = false);
    void reset();
    float minutes(const Settings &) const;
};
