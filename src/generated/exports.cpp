// Generated x86 tail-jump forwarders. Preserve argument/return ABI, including private exports.
#include "exports.hpp"
extern "C" void Resolve_Ordinal16();
extern "C" { void* target_Ordinal16 = reinterpret_cast<void*>(&Resolve_Ordinal16); }
extern "C" __declspec(naked) void Resolve_Ordinal16() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { pushfd }
    __asm { pushad }
    __asm { mov ebx, esp }
    __asm { and esp, -16 }
    __asm { call EnsureSystem }
    __asm { mov esp, ebx }
    __asm { popad }
    __asm { popfd }
    __asm { jmp dword ptr [target_Ordinal16] }
#else
    __asm__ volatile("pushfl; pushal; movl %esp, %ebx; andl $-16, %esp; call _EnsureSystem; movl %ebx, %esp; popal; popfl; jmp *_target_Ordinal16");
#endif
}
extern "C" __declspec(naked) void FENB_Ordinal16() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { jmp dword ptr [target_Ordinal16] }
#else
    __asm__ volatile("jmp *_target_Ordinal16");
#endif
}
extern "C" void Resolve_Ordinal17();
extern "C" { void* target_Ordinal17 = reinterpret_cast<void*>(&Resolve_Ordinal17); }
extern "C" __declspec(naked) void Resolve_Ordinal17() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { pushfd }
    __asm { pushad }
    __asm { mov ebx, esp }
    __asm { and esp, -16 }
    __asm { call EnsureSystem }
    __asm { mov esp, ebx }
    __asm { popad }
    __asm { popfd }
    __asm { jmp dword ptr [target_Ordinal17] }
#else
    __asm__ volatile("pushfl; pushal; movl %esp, %ebx; andl $-16, %esp; call _EnsureSystem; movl %ebx, %esp; popal; popfl; jmp *_target_Ordinal17");
#endif
}
extern "C" __declspec(naked) void FENB_Ordinal17() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { jmp dword ptr [target_Ordinal17] }
#else
    __asm__ volatile("jmp *_target_Ordinal17");
#endif
}
extern "C" void Resolve_Ordinal18();
extern "C" { void* target_Ordinal18 = reinterpret_cast<void*>(&Resolve_Ordinal18); }
extern "C" __declspec(naked) void Resolve_Ordinal18() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { pushfd }
    __asm { pushad }
    __asm { mov ebx, esp }
    __asm { and esp, -16 }
    __asm { call EnsureSystem }
    __asm { mov esp, ebx }
    __asm { popad }
    __asm { popfd }
    __asm { jmp dword ptr [target_Ordinal18] }
#else
    __asm__ volatile("pushfl; pushal; movl %esp, %ebx; andl $-16, %esp; call _EnsureSystem; movl %ebx, %esp; popal; popfl; jmp *_target_Ordinal18");
#endif
}
extern "C" __declspec(naked) void FENB_Ordinal18() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { jmp dword ptr [target_Ordinal18] }
#else
    __asm__ volatile("jmp *_target_Ordinal18");
#endif
}
extern "C" void Resolve_Ordinal19();
extern "C" { void* target_Ordinal19 = reinterpret_cast<void*>(&Resolve_Ordinal19); }
extern "C" __declspec(naked) void Resolve_Ordinal19() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { pushfd }
    __asm { pushad }
    __asm { mov ebx, esp }
    __asm { and esp, -16 }
    __asm { call EnsureSystem }
    __asm { mov esp, ebx }
    __asm { popad }
    __asm { popfd }
    __asm { jmp dword ptr [target_Ordinal19] }
#else
    __asm__ volatile("pushfl; pushal; movl %esp, %ebx; andl $-16, %esp; call _EnsureSystem; movl %ebx, %esp; popal; popfl; jmp *_target_Ordinal19");
#endif
}
extern "C" __declspec(naked) void FENB_Ordinal19() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { jmp dword ptr [target_Ordinal19] }
#else
    __asm__ volatile("jmp *_target_Ordinal19");
#endif
}
extern "C" void Resolve_Ordinal22();
extern "C" { void* target_Ordinal22 = reinterpret_cast<void*>(&Resolve_Ordinal22); }
extern "C" __declspec(naked) void Resolve_Ordinal22() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { pushfd }
    __asm { pushad }
    __asm { mov ebx, esp }
    __asm { and esp, -16 }
    __asm { call EnsureSystem }
    __asm { mov esp, ebx }
    __asm { popad }
    __asm { popfd }
    __asm { jmp dword ptr [target_Ordinal22] }
#else
    __asm__ volatile("pushfl; pushal; movl %esp, %ebx; andl $-16, %esp; call _EnsureSystem; movl %ebx, %esp; popal; popfl; jmp *_target_Ordinal22");
#endif
}
extern "C" __declspec(naked) void FENB_Ordinal22() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { jmp dword ptr [target_Ordinal22] }
#else
    __asm__ volatile("jmp *_target_Ordinal22");
#endif
}
extern "C" void Resolve_Ordinal23();
extern "C" { void* target_Ordinal23 = reinterpret_cast<void*>(&Resolve_Ordinal23); }
extern "C" __declspec(naked) void Resolve_Ordinal23() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { pushfd }
    __asm { pushad }
    __asm { mov ebx, esp }
    __asm { and esp, -16 }
    __asm { call EnsureSystem }
    __asm { mov esp, ebx }
    __asm { popad }
    __asm { popfd }
    __asm { jmp dword ptr [target_Ordinal23] }
#else
    __asm__ volatile("pushfl; pushal; movl %esp, %ebx; andl $-16, %esp; call _EnsureSystem; movl %ebx, %esp; popal; popfl; jmp *_target_Ordinal23");
#endif
}
extern "C" __declspec(naked) void FENB_Ordinal23() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { jmp dword ptr [target_Ordinal23] }
#else
    __asm__ volatile("jmp *_target_Ordinal23");
#endif
}
extern "C" void Resolve_Direct3DShaderValidatorCreate9();
extern "C" { void* target_Direct3DShaderValidatorCreate9 = reinterpret_cast<void*>(&Resolve_Direct3DShaderValidatorCreate9); }
extern "C" __declspec(naked) void Resolve_Direct3DShaderValidatorCreate9() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { pushfd }
    __asm { pushad }
    __asm { mov ebx, esp }
    __asm { and esp, -16 }
    __asm { call EnsureSystem }
    __asm { mov esp, ebx }
    __asm { popad }
    __asm { popfd }
    __asm { jmp dword ptr [target_Direct3DShaderValidatorCreate9] }
#else
    __asm__ volatile("pushfl; pushal; movl %esp, %ebx; andl $-16, %esp; call _EnsureSystem; movl %ebx, %esp; popal; popfl; jmp *_target_Direct3DShaderValidatorCreate9");
#endif
}
extern "C" __declspec(naked) void FENB_Direct3DShaderValidatorCreate9() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { jmp dword ptr [target_Direct3DShaderValidatorCreate9] }
#else
    __asm__ volatile("jmp *_target_Direct3DShaderValidatorCreate9");
#endif
}
extern "C" void Resolve_PSGPError();
extern "C" { void* target_PSGPError = reinterpret_cast<void*>(&Resolve_PSGPError); }
extern "C" __declspec(naked) void Resolve_PSGPError() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { pushfd }
    __asm { pushad }
    __asm { mov ebx, esp }
    __asm { and esp, -16 }
    __asm { call EnsureSystem }
    __asm { mov esp, ebx }
    __asm { popad }
    __asm { popfd }
    __asm { jmp dword ptr [target_PSGPError] }
#else
    __asm__ volatile("pushfl; pushal; movl %esp, %ebx; andl $-16, %esp; call _EnsureSystem; movl %ebx, %esp; popal; popfl; jmp *_target_PSGPError");
#endif
}
extern "C" __declspec(naked) void FENB_PSGPError() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { jmp dword ptr [target_PSGPError] }
#else
    __asm__ volatile("jmp *_target_PSGPError");
#endif
}
extern "C" void Resolve_PSGPSampleTexture();
extern "C" { void* target_PSGPSampleTexture = reinterpret_cast<void*>(&Resolve_PSGPSampleTexture); }
extern "C" __declspec(naked) void Resolve_PSGPSampleTexture() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { pushfd }
    __asm { pushad }
    __asm { mov ebx, esp }
    __asm { and esp, -16 }
    __asm { call EnsureSystem }
    __asm { mov esp, ebx }
    __asm { popad }
    __asm { popfd }
    __asm { jmp dword ptr [target_PSGPSampleTexture] }
#else
    __asm__ volatile("pushfl; pushal; movl %esp, %ebx; andl $-16, %esp; call _EnsureSystem; movl %ebx, %esp; popal; popfl; jmp *_target_PSGPSampleTexture");
#endif
}
extern "C" __declspec(naked) void FENB_PSGPSampleTexture() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { jmp dword ptr [target_PSGPSampleTexture] }
#else
    __asm__ volatile("jmp *_target_PSGPSampleTexture");
#endif
}
extern "C" void Resolve_D3DPERF_BeginEvent();
extern "C" { void* target_D3DPERF_BeginEvent = reinterpret_cast<void*>(&Resolve_D3DPERF_BeginEvent); }
extern "C" __declspec(naked) void Resolve_D3DPERF_BeginEvent() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { pushfd }
    __asm { pushad }
    __asm { mov ebx, esp }
    __asm { and esp, -16 }
    __asm { call EnsureSystem }
    __asm { mov esp, ebx }
    __asm { popad }
    __asm { popfd }
    __asm { jmp dword ptr [target_D3DPERF_BeginEvent] }
#else
    __asm__ volatile("pushfl; pushal; movl %esp, %ebx; andl $-16, %esp; call _EnsureSystem; movl %ebx, %esp; popal; popfl; jmp *_target_D3DPERF_BeginEvent");
#endif
}
extern "C" __declspec(naked) void FENB_D3DPERF_BeginEvent() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { jmp dword ptr [target_D3DPERF_BeginEvent] }
#else
    __asm__ volatile("jmp *_target_D3DPERF_BeginEvent");
#endif
}
extern "C" void Resolve_D3DPERF_EndEvent();
extern "C" { void* target_D3DPERF_EndEvent = reinterpret_cast<void*>(&Resolve_D3DPERF_EndEvent); }
extern "C" __declspec(naked) void Resolve_D3DPERF_EndEvent() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { pushfd }
    __asm { pushad }
    __asm { mov ebx, esp }
    __asm { and esp, -16 }
    __asm { call EnsureSystem }
    __asm { mov esp, ebx }
    __asm { popad }
    __asm { popfd }
    __asm { jmp dword ptr [target_D3DPERF_EndEvent] }
#else
    __asm__ volatile("pushfl; pushal; movl %esp, %ebx; andl $-16, %esp; call _EnsureSystem; movl %ebx, %esp; popal; popfl; jmp *_target_D3DPERF_EndEvent");
#endif
}
extern "C" __declspec(naked) void FENB_D3DPERF_EndEvent() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { jmp dword ptr [target_D3DPERF_EndEvent] }
#else
    __asm__ volatile("jmp *_target_D3DPERF_EndEvent");
#endif
}
extern "C" void Resolve_D3DPERF_GetStatus();
extern "C" { void* target_D3DPERF_GetStatus = reinterpret_cast<void*>(&Resolve_D3DPERF_GetStatus); }
extern "C" __declspec(naked) void Resolve_D3DPERF_GetStatus() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { pushfd }
    __asm { pushad }
    __asm { mov ebx, esp }
    __asm { and esp, -16 }
    __asm { call EnsureSystem }
    __asm { mov esp, ebx }
    __asm { popad }
    __asm { popfd }
    __asm { jmp dword ptr [target_D3DPERF_GetStatus] }
#else
    __asm__ volatile("pushfl; pushal; movl %esp, %ebx; andl $-16, %esp; call _EnsureSystem; movl %ebx, %esp; popal; popfl; jmp *_target_D3DPERF_GetStatus");
#endif
}
extern "C" __declspec(naked) void FENB_D3DPERF_GetStatus() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { jmp dword ptr [target_D3DPERF_GetStatus] }
#else
    __asm__ volatile("jmp *_target_D3DPERF_GetStatus");
#endif
}
extern "C" void Resolve_D3DPERF_QueryRepeatFrame();
extern "C" { void* target_D3DPERF_QueryRepeatFrame = reinterpret_cast<void*>(&Resolve_D3DPERF_QueryRepeatFrame); }
extern "C" __declspec(naked) void Resolve_D3DPERF_QueryRepeatFrame() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { pushfd }
    __asm { pushad }
    __asm { mov ebx, esp }
    __asm { and esp, -16 }
    __asm { call EnsureSystem }
    __asm { mov esp, ebx }
    __asm { popad }
    __asm { popfd }
    __asm { jmp dword ptr [target_D3DPERF_QueryRepeatFrame] }
#else
    __asm__ volatile("pushfl; pushal; movl %esp, %ebx; andl $-16, %esp; call _EnsureSystem; movl %ebx, %esp; popal; popfl; jmp *_target_D3DPERF_QueryRepeatFrame");
#endif
}
extern "C" __declspec(naked) void FENB_D3DPERF_QueryRepeatFrame() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { jmp dword ptr [target_D3DPERF_QueryRepeatFrame] }
#else
    __asm__ volatile("jmp *_target_D3DPERF_QueryRepeatFrame");
#endif
}
extern "C" void Resolve_D3DPERF_SetMarker();
extern "C" { void* target_D3DPERF_SetMarker = reinterpret_cast<void*>(&Resolve_D3DPERF_SetMarker); }
extern "C" __declspec(naked) void Resolve_D3DPERF_SetMarker() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { pushfd }
    __asm { pushad }
    __asm { mov ebx, esp }
    __asm { and esp, -16 }
    __asm { call EnsureSystem }
    __asm { mov esp, ebx }
    __asm { popad }
    __asm { popfd }
    __asm { jmp dword ptr [target_D3DPERF_SetMarker] }
#else
    __asm__ volatile("pushfl; pushal; movl %esp, %ebx; andl $-16, %esp; call _EnsureSystem; movl %ebx, %esp; popal; popfl; jmp *_target_D3DPERF_SetMarker");
#endif
}
extern "C" __declspec(naked) void FENB_D3DPERF_SetMarker() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { jmp dword ptr [target_D3DPERF_SetMarker] }
#else
    __asm__ volatile("jmp *_target_D3DPERF_SetMarker");
#endif
}
extern "C" void Resolve_D3DPERF_SetOptions();
extern "C" { void* target_D3DPERF_SetOptions = reinterpret_cast<void*>(&Resolve_D3DPERF_SetOptions); }
extern "C" __declspec(naked) void Resolve_D3DPERF_SetOptions() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { pushfd }
    __asm { pushad }
    __asm { mov ebx, esp }
    __asm { and esp, -16 }
    __asm { call EnsureSystem }
    __asm { mov esp, ebx }
    __asm { popad }
    __asm { popfd }
    __asm { jmp dword ptr [target_D3DPERF_SetOptions] }
#else
    __asm__ volatile("pushfl; pushal; movl %esp, %ebx; andl $-16, %esp; call _EnsureSystem; movl %ebx, %esp; popal; popfl; jmp *_target_D3DPERF_SetOptions");
#endif
}
extern "C" __declspec(naked) void FENB_D3DPERF_SetOptions() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { jmp dword ptr [target_D3DPERF_SetOptions] }
#else
    __asm__ volatile("jmp *_target_D3DPERF_SetOptions");
#endif
}
extern "C" void Resolve_D3DPERF_SetRegion();
extern "C" { void* target_D3DPERF_SetRegion = reinterpret_cast<void*>(&Resolve_D3DPERF_SetRegion); }
extern "C" __declspec(naked) void Resolve_D3DPERF_SetRegion() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { pushfd }
    __asm { pushad }
    __asm { mov ebx, esp }
    __asm { and esp, -16 }
    __asm { call EnsureSystem }
    __asm { mov esp, ebx }
    __asm { popad }
    __asm { popfd }
    __asm { jmp dword ptr [target_D3DPERF_SetRegion] }
#else
    __asm__ volatile("pushfl; pushal; movl %esp, %ebx; andl $-16, %esp; call _EnsureSystem; movl %ebx, %esp; popal; popfl; jmp *_target_D3DPERF_SetRegion");
#endif
}
extern "C" __declspec(naked) void FENB_D3DPERF_SetRegion() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { jmp dword ptr [target_D3DPERF_SetRegion] }
#else
    __asm__ volatile("jmp *_target_D3DPERF_SetRegion");
#endif
}
extern "C" void Resolve_DebugSetLevel();
extern "C" { void* target_DebugSetLevel = reinterpret_cast<void*>(&Resolve_DebugSetLevel); }
extern "C" __declspec(naked) void Resolve_DebugSetLevel() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { pushfd }
    __asm { pushad }
    __asm { mov ebx, esp }
    __asm { and esp, -16 }
    __asm { call EnsureSystem }
    __asm { mov esp, ebx }
    __asm { popad }
    __asm { popfd }
    __asm { jmp dword ptr [target_DebugSetLevel] }
#else
    __asm__ volatile("pushfl; pushal; movl %esp, %ebx; andl $-16, %esp; call _EnsureSystem; movl %ebx, %esp; popal; popfl; jmp *_target_DebugSetLevel");
#endif
}
extern "C" __declspec(naked) void FENB_DebugSetLevel() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { jmp dword ptr [target_DebugSetLevel] }
#else
    __asm__ volatile("jmp *_target_DebugSetLevel");
#endif
}
extern "C" void Resolve_DebugSetMute();
extern "C" { void* target_DebugSetMute = reinterpret_cast<void*>(&Resolve_DebugSetMute); }
extern "C" __declspec(naked) void Resolve_DebugSetMute() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { pushfd }
    __asm { pushad }
    __asm { mov ebx, esp }
    __asm { and esp, -16 }
    __asm { call EnsureSystem }
    __asm { mov esp, ebx }
    __asm { popad }
    __asm { popfd }
    __asm { jmp dword ptr [target_DebugSetMute] }
#else
    __asm__ volatile("pushfl; pushal; movl %esp, %ebx; andl $-16, %esp; call _EnsureSystem; movl %ebx, %esp; popal; popfl; jmp *_target_DebugSetMute");
#endif
}
extern "C" __declspec(naked) void FENB_DebugSetMute() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { jmp dword ptr [target_DebugSetMute] }
#else
    __asm__ volatile("jmp *_target_DebugSetMute");
#endif
}
extern "C" void Resolve_Direct3D9EnableMaximizedWindowedModeShim();
extern "C" { void* target_Direct3D9EnableMaximizedWindowedModeShim = reinterpret_cast<void*>(&Resolve_Direct3D9EnableMaximizedWindowedModeShim); }
extern "C" __declspec(naked) void Resolve_Direct3D9EnableMaximizedWindowedModeShim() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { pushfd }
    __asm { pushad }
    __asm { mov ebx, esp }
    __asm { and esp, -16 }
    __asm { call EnsureSystem }
    __asm { mov esp, ebx }
    __asm { popad }
    __asm { popfd }
    __asm { jmp dword ptr [target_Direct3D9EnableMaximizedWindowedModeShim] }
#else
    __asm__ volatile("pushfl; pushal; movl %esp, %ebx; andl $-16, %esp; call _EnsureSystem; movl %ebx, %esp; popal; popfl; jmp *_target_Direct3D9EnableMaximizedWindowedModeShim");
#endif
}
extern "C" __declspec(naked) void FENB_Direct3D9EnableMaximizedWindowedModeShim() {
#if defined(_MSC_VER) && !defined(__clang__)
    __asm { jmp dword ptr [target_Direct3D9EnableMaximizedWindowedModeShim] }
#else
    __asm__ volatile("jmp *_target_Direct3D9EnableMaximizedWindowedModeShim");
#endif
}
void BindForwarders(HMODULE module) {
    target_Ordinal16 = reinterpret_cast<void*>(GetProcAddress(module, MAKEINTRESOURCEA(16)));
    target_Ordinal17 = reinterpret_cast<void*>(GetProcAddress(module, MAKEINTRESOURCEA(17)));
    target_Ordinal18 = reinterpret_cast<void*>(GetProcAddress(module, MAKEINTRESOURCEA(18)));
    target_Ordinal19 = reinterpret_cast<void*>(GetProcAddress(module, MAKEINTRESOURCEA(19)));
    target_Ordinal22 = reinterpret_cast<void*>(GetProcAddress(module, MAKEINTRESOURCEA(22)));
    target_Ordinal23 = reinterpret_cast<void*>(GetProcAddress(module, MAKEINTRESOURCEA(23)));
    target_Direct3DShaderValidatorCreate9 = reinterpret_cast<void*>(GetProcAddress(module, "Direct3DShaderValidatorCreate9"));
    target_PSGPError = reinterpret_cast<void*>(GetProcAddress(module, "PSGPError"));
    target_PSGPSampleTexture = reinterpret_cast<void*>(GetProcAddress(module, "PSGPSampleTexture"));
    target_D3DPERF_BeginEvent = reinterpret_cast<void*>(GetProcAddress(module, "D3DPERF_BeginEvent"));
    target_D3DPERF_EndEvent = reinterpret_cast<void*>(GetProcAddress(module, "D3DPERF_EndEvent"));
    target_D3DPERF_GetStatus = reinterpret_cast<void*>(GetProcAddress(module, "D3DPERF_GetStatus"));
    target_D3DPERF_QueryRepeatFrame = reinterpret_cast<void*>(GetProcAddress(module, "D3DPERF_QueryRepeatFrame"));
    target_D3DPERF_SetMarker = reinterpret_cast<void*>(GetProcAddress(module, "D3DPERF_SetMarker"));
    target_D3DPERF_SetOptions = reinterpret_cast<void*>(GetProcAddress(module, "D3DPERF_SetOptions"));
    target_D3DPERF_SetRegion = reinterpret_cast<void*>(GetProcAddress(module, "D3DPERF_SetRegion"));
    target_DebugSetLevel = reinterpret_cast<void*>(GetProcAddress(module, "DebugSetLevel"));
    target_DebugSetMute = reinterpret_cast<void*>(GetProcAddress(module, "DebugSetMute"));
    target_Direct3D9EnableMaximizedWindowedModeShim = reinterpret_cast<void*>(GetProcAddress(module, "Direct3D9EnableMaximizedWindowedModeShim"));
}
