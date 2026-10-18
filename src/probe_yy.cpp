// Read-only Nova Drift probe. Build against the pinned Zeus experimental SDK.
// Engine calls run only from engine input detours / Code_Execute callbacks.
#include <windows.h>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <YYToolkit/YYTK_Shared.hpp>

using namespace Aurie;
using namespace YYTK;

namespace {
YYTKInterface* api = nullptr;
std::filesystem::path log_path;
std::filesystem::path runtime_root;
std::mutex log_mutex;
std::atomic<ULONGLONG> last_observation{0};
std::atomic<ULONGLONG> last_frame{0};
thread_local bool observing = false;
TRoutine keyboard_check_original = nullptr;
TRoutine keyboard_pressed_original = nullptr;
TRoutine keyboard_direct_original = nullptr;
TRoutine gamepad_axis_original = nullptr;

void Log(const char* format, ...) {
    std::lock_guard<std::mutex> guard(log_mutex);
    FILE* output = nullptr;
    if (_wfopen_s(&output, log_path.c_str(), L"a") || !output) return;
    std::fprintf(output, "[%llu thread=%lu] ", GetTickCount64(), GetCurrentThreadId());
    va_list args;
    va_start(args, format);
    std::vfprintf(output, format, args);
    va_end(args);
    std::fputc('\n', output);
    std::fclose(output);
}

void ReadObject(const char* name) {
    const RValue object = api->CallBuiltin("asset_get_index", {RValue(name)});
    const int object_index = object.ToInt32();
    if (object_index < 0) {
        Log("object=%s asset_get_index=%d", name, object_index);
        return;
    }
    const int count = api->CallBuiltin("instance_number", {object}).ToInt32();
    Log("object=%s index=%d instances=%d", name, object_index, count);
    // No cached instance pointer survives this observation or a room transition.
    for (int i = 0; i < count && i < 4; ++i) {
        const int id = api->CallBuiltin("instance_find", {object, RValue(i)}).ToInt32();
        CInstance* instance = nullptr;
        const auto status = api->GetInstanceObject(id, instance);
        if (!AurieSuccess(status) || !instance) {
            Log("object=%s id=%d GetInstanceObject=%s", name, id, AurieStatusToString(status));
            continue;
        }
        RValue x, y;
        const auto xs = api->GetBuiltin("x", instance, NULL_INDEX, x);
        const auto ys = api->GetBuiltin("y", instance, NULL_INDEX, y);
        if (AurieSuccess(xs) && AurieSuccess(ys)) {
            Log("object=%s id=%d pointer=%p x=%.3f y=%.3f", name, id, instance, x.ToDouble(), y.ToDouble());
        } else {
            Log("object=%s id=%d x_status=%s y_status=%s", name, id, AurieStatusToString(xs), AurieStatusToString(ys));
        }
    }
}

void Observe(const char* origin) {
    if (observing || !api) return;
    const ULONGLONG now = GetTickCount64();
    auto previous = last_observation.load();
    if (now - previous < 2000 || !last_observation.compare_exchange_strong(previous, now)) return;
    observing = true;
    Log("engine observation via %s", origin);
    static bool paths_logged = false;
    if (!paths_logged) {
        for (const char* name : {"working_directory", "program_directory", "temp_directory", "game_save_id"}) {
            RValue value;
            const auto status = api->GetBuiltin(name, nullptr, NULL_INDEX, value);
            if (AurieSuccess(status)) Log("builtin path %s=%s", name, value.ToString().c_str());
            else Log("builtin path %s status=%s", name, AurieStatusToString(status));
        }
        paths_logged = true;
    }
    ReadObject("oPlayer");
    ReadObject("oConstructAlly");
    static unsigned int observations = 0;
    static std::string screenshot_command;
    ++observations;
    std::ifstream trigger(runtime_root / L"probe-screen.txt");
    std::string contents((std::istreambuf_iterator<char>(trigger)), {});
    const bool requested = !contents.empty() && contents != screenshot_command;
    if (observations == 3 || requested) {
        screenshot_command = contents;
        RValue result;
        const auto status = api->CallBuiltinEx(result, "screen_save", nullptr, nullptr, {RValue("probe-menu.png")});
        Log("screen_save probe-menu.png in game_save_id status=%s (verify actual PNG)", AurieStatusToString(status));
    }
    observing = false;
}

void KeyboardCheck(RValue& result, CInstance* self, CInstance* other, int argc, RValue* argv) {
    keyboard_check_original(result, self, other, argc, argv);
    Observe("keyboard_check");
}
void KeyboardPressed(RValue& result, CInstance* self, CInstance* other, int argc, RValue* argv) {
    keyboard_pressed_original(result, self, other, argc, argv);
    Observe("keyboard_check_pressed");
}
void KeyboardDirect(RValue& result, CInstance* self, CInstance* other, int argc, RValue* argv) {
    keyboard_direct_original(result, self, other, argc, argv);
    Observe("keyboard_check_direct");
}
void GamepadAxis(RValue& result, CInstance* self, CInstance* other, int argc, RValue* argv) {
    gamepad_axis_original(result, self, other, argc, argv);
    Observe("gamepad_axis_value");
}

void CodeCallback(FWCodeEvent&) { Observe("Code_Execute"); }
void FrameCallback(FWFrame&) {
    // Present may be on another thread: this is only a loader/render heartbeat.
    const auto now = GetTickCount64();
    auto previous = last_frame.load();
    if (now - previous > 5000 && last_frame.compare_exchange_strong(previous, now)) Log("render heartbeat");
}

void HookInput(AurieModule* module, const char* name, TRoutine detour, TRoutine& original) {
    void* target = nullptr;
    auto status = api->GetNamedRoutinePointer(name, &target);
    if (AurieSuccess(status) && target) {
        status = MmCreateHook(module, name, target, reinterpret_cast<void*>(detour), reinterpret_cast<void**>(&original));
    }
    Log("input hook=%s target=%p status=%s", name, target, AurieStatusToString(status));
}
}

EXPORTED AurieStatus ModuleInitialize(AurieModule* module, const fs::path& module_path) {
    log_path = module_path.parent_path() / L"NovaDriftProbe.log";
    runtime_root = module_path.parent_path().parent_path().parent_path();
    Log("ModuleInitialize probe=2 sdk=experimental-d5cc0078 compiled=%s %s", __DATE__, __TIME__);
    api = GetInterface();
    if (!api) {
        Log("YYTK_ZeusMain interface unavailable");
        return AURIE_MODULE_DEPENDENCY_NOT_RESOLVED;
    }
    short major = 0, minor = 0, patch = 0;
    api->QueryVersion(major, minor, patch);
    Log("YYTK interface version=%d.%d.%d", major, minor, patch);
    if (major != 5) return AURIE_MODULE_DEPENDENCY_NOT_RESOLVED;
    auto status = api->CreateCallback(module, EVENT_OBJECT_CALL, reinterpret_cast<void*>(CodeCallback), 0);
    Log("Code_Execute callback status=%s", AurieStatusToString(status));
    status = api->CreateCallback(module, EVENT_FRAME, reinterpret_cast<void*>(FrameCallback), 0);
    Log("Present callback status=%s", AurieStatusToString(status));
    HookInput(module, "keyboard_check", KeyboardCheck, keyboard_check_original);
    HookInput(module, "keyboard_check_pressed", KeyboardPressed, keyboard_pressed_original);
    HookInput(module, "keyboard_check_direct", KeyboardDirect, keyboard_direct_original);
    HookInput(module, "gamepad_axis_value", GamepadAxis, gamepad_axis_original);
    return AURIE_SUCCESS;
}
