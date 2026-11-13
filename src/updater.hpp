#pragma once
#include <windows.h>
#include <string>
#include "localization.hpp"

enum class UpdateState { Idle, Checking, Available, Current, Downloading, Ready, RollbackReady,
                         Installing, Installed, Restored, Failed };
struct UpdateStatus {
    UpdateState state = UpdateState::Idle;
    std::string version;
    bool rollback = false;
};
// Only local process creation / bounded status reads happen here. Network and file
// transactions run in the embedded PowerShell worker, outside the rendering thread.
class Updater {
  public:
    Updater() = default;
    ~Updater();
    Updater(const Updater &) = delete;
    Updater &operator=(const Updater &) = delete;
    void menuOpened(Language language);
    void poll();
    void check(Language language);
    void install(Language language);
    void rollback(Language language);
    const UpdateStatus &status() const { return status_; }
    bool busy() const;
  private:
    friend struct UpdateTestAccess;
    void start(const wchar_t *mode, UpdateState pending, Language language = Language::English);
    UpdateStatus status_;
    HANDLE process_ = nullptr;
    DWORD lastPoll_ = 0;
    bool opened_ = false;
    bool localFailure_ = false;
};
