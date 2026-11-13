#include "updater.hpp"
#include "common.hpp"
#include "version.hpp"
#include "generated/update_worker.hpp"
#include <cstdio>

namespace {
std::wstring Literal(const std::wstring &value) {
    std::wstring result = L"'";
    for (wchar_t c : value) { result += c; if (c == L'\'') result += c; }
    return result + L"'";
}
std::wstring Base64(const std::wstring &s) {
    constexpr wchar_t alphabet[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const auto *data = reinterpret_cast<const unsigned char *>(s.data());
    const size_t bytes = s.size() * sizeof(wchar_t);
    std::wstring result;
    for (size_t i = 0; i < bytes; i += 3) {
        unsigned v = unsigned(data[i]) << 16;
        if (i + 1 < bytes) v |= unsigned(data[i+1]) << 8;
        if (i + 2 < bytes) v |= data[i+2];
        result += alphabet[v >> 18]; result += alphabet[(v >> 12) & 63];
        result += i+1 < bytes ? alphabet[(v >> 6) & 63] : L'=';
        result += i+2 < bytes ? alphabet[v & 63] : L'=';
    }
    return result;
}
std::wstring Wide(const char *s) {
    int size = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s, -1, result.data(), size);
    if (!result.empty()) result.pop_back();
    return result;
}
}
Updater::~Updater() { if (process_) CloseHandle(process_); }
bool Updater::busy() const {
    return process_ || status_.state == UpdateState::Checking || status_.state == UpdateState::Downloading ||
           status_.state == UpdateState::Ready || status_.state == UpdateState::RollbackReady ||
           status_.state == UpdateState::Installing;
}
void Updater::menuOpened(Language language) {
#ifdef FENB_UPDATER_OFFLINE_TESTS
    return;
#endif
    if (opened_) return;
    opened_ = true;
    lastPoll_ = GetTickCount() - 1000;
    poll();
    if (!busy()) check(language);
}
void Updater::poll() {
#ifdef FENB_UPDATER_OFFLINE_TESTS
    return;
#endif
    if (localFailure_) return;
    auto now = GetTickCount();
    if (now - lastPoll_ < 500) return;
    lastPoll_ = now;
    auto directory = ModuleDirectory() + L"FENB-update\\";
    status_.rollback = GetFileAttributesW((directory + L"rollback.json").c_str()) != INVALID_FILE_ATTRIBUTES &&
                       GetFileAttributesW((directory + L"rollback.dll").c_str()) != INVALID_FILE_ATTRIBUTES;
    FILE *file = _wfopen((directory + L"state.txt").c_str(), L"rb");
    if (file) {
        char state[32]{}, version[16]{};
        if (fgets(state, sizeof(state), file)) {
            state[strcspn(state, "\r\n")] = 0;
            (void)fgets(version, sizeof(version), file);
            version[strcspn(version, "\r\n")] = 0;
            const char *names[] = {"Idle","Checking","Available","Current","Downloading","Ready",
                                   "RollbackReady","Installing","Installed","Restored","Failed"};
            for (unsigned i = 0; i < sizeof(names)/sizeof(*names); ++i)
                if (!strcmp(state,names[i])) status_.state = static_cast<UpdateState>(i);
            if (strlen(version) == 4 && version[0] >= '0' && version[0] <= '9' && version[1] == '.' &&
                version[2] >= '0' && version[2] <= '9' && version[3] >= 'a' && version[3] <= 'h')
                status_.version = version;
            else status_.version.clear();
        }
        fclose(file);
    }
    if (process_ && WaitForSingleObject(process_, 0) == WAIT_OBJECT_0) {
        DWORD code = 1;
        GetExitCodeProcess(process_, &code);
        CloseHandle(process_); process_ = nullptr;
        if (code != 0 || busy()) status_.state = UpdateState::Failed;
    }
    if (!process_ && busy()) {
        HANDLE lock = CreateFileW((directory + L"lock").c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (lock != INVALID_HANDLE_VALUE) { CloseHandle(lock); status_.state = UpdateState::Failed; }
        else if (GetLastError() != ERROR_SHARING_VIOLATION) status_.state = UpdateState::Failed;
    }
}
void Updater::start(const wchar_t *mode, UpdateState pending, Language language) {
    poll();
    if (busy()) return;
    auto root = ModuleDirectory(), work = root + L"FENB-update\\";
    if (!CreateDirectoryW(work.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
        status_.state = UpdateState::Failed; localFailure_ = true; return;
    }
    const auto script = work + L"worker.ps1";
    const auto temporary = work + L"worker-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                           std::to_wstring(GetCurrentThreadId()) + L".tmp";
    FILE *file = _wfopen(temporary.c_str(), L"wb");
    if (!file) { status_.state = UpdateState::Failed; localFailure_ = true; return; }
    bool written = fwrite(UpdateWorker,1,sizeof(UpdateWorker)-1,file) == sizeof(UpdateWorker)-1;
    if (fclose(file)) written = false;
    if (!written || !MoveFileExW(temporary.c_str(),script.c_str(),MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temporary.c_str());
        status_.state = UpdateState::Failed; localFailure_ = true; return;
    }
    wchar_t system[MAX_PATH]{};
    if (!GetSystemDirectoryW(system, MAX_PATH)) { status_.state = UpdateState::Failed; localFailure_ = true; return; }
    auto executable = std::wstring(system) + L"\\WindowsPowerShell\\v1.0\\powershell.exe";
    FILETIME created{}, exited{}, kernel{}, user{};
    GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user);
    ULARGE_INTEGER start{}; start.LowPart = created.dwLowDateTime; start.HighPart = created.dwHighDateTime;
    auto command = L"& " + Literal(script) + L" -Mode " + mode + L" -Root " + Literal(root) +
        L" -CurrentVersion '" + std::wstring(FENBVersion,FENBVersion+strlen(FENBVersion)) +
        L"' -OwnerId " + std::to_wstring(GetCurrentProcessId()) + L" -OwnerStart " + std::to_wstring(start.QuadPart) +
        L" -InstalledMessage " + Literal(Wide(Translate(language,UiText::UpdateFinishedNotice))) +
        L" -RestoredMessage " + Literal(Wide(Translate(language,UiText::RollbackFinishedNotice))) +
        L" -FailureMessage " + Literal(Wide(Translate(language,UiText::UpdateFailureNotice)));
    auto args = L"\"" + executable + L"\" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand " + Base64(command);
    STARTUPINFOW info{}; info.cb = sizeof(info); info.dwFlags = STARTF_USESHOWWINDOW; info.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(), args.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                        nullptr, root.c_str(), &info, &process)) {
        status_.state = UpdateState::Failed; localFailure_ = true; return;
    }
    CloseHandle(process.hThread); process_ = process.hProcess;
    status_.state = pending;
    localFailure_ = false;
    // The child publishes its initial state before the first normal poll.
    lastPoll_ = GetTickCount();
}
void Updater::check(Language language) { start(L"Check", UpdateState::Checking, language); }
void Updater::install(Language language) { if (status_.state == UpdateState::Available) start(L"Install", UpdateState::Downloading, language); }
void Updater::rollback(Language language) { if (status_.rollback) start(L"Rollback", UpdateState::Downloading, language); }
