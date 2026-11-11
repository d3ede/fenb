#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cmath>

template<class T> void Drop(T*& p) { if (p) { p->Release(); p = nullptr; } }
template<class T> struct Com {
    T* p = nullptr;
    ~Com() { Drop(p); }
    Com() = default;
    Com(const Com&) = delete;
    Com& operator=(const Com&) = delete;
    T* operator->() const { return p; }
    operator T*() const { return p; }
    T** out() { Drop(p); return &p; }
};
std::wstring ModuleDirectory();
void Log(const char* text);

