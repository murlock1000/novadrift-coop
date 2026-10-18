// Local player-two Ally prototype, for the pinned Nova Drift build only.
// All engine reads/writes and script invocations happen on native event threads.
#define NOMINMAX
#include <windows.h>
#include <Xinput.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <YYToolkit/YYTK_Shared.hpp>
#include "native_abi.hpp"

using namespace Aurie;
using namespace YYTK;
namespace nd = novadrift_native;

namespace {
YYTKInterface* api = nullptr;
nd::Access access{};
nd::Event original_step = nullptr, original_alarm0 = nullptr;
nd::Event original_alarm2 = nullptr, original_player_step = nullptr;
nd::Event original_player_create = nullptr;
nd::Event original_projectile_create = nullptr;
nd::Event original_projectile_collision = nullptr;
nd::Event original_solid_drone_collision = nullptr;
nd::Script original_restart_game = nullptr;
// The SDK's FWCodeEvent alias reverses these final two types. Match the actual
// Zeus HkExecuteIt implementation before invoking the original wrapper.
using NativeCodeEvent = FunctionWrapper<bool(CInstance*, CInstance*, CCode*, RValue*, int)>;
TRoutine original_connected = nullptr, original_axis = nullptr, original_button = nullptr;
TRoutine original_pressed = nullptr, original_released = nullptr, original_button_value = nullptr;
TRoutine original_focus = nullptr;
std::filesystem::path runtime_root;
FILE* events = nullptr;
FILE* telemetry = nullptr;
nd::Instance* selected = nullptr;
int selected_id = -1;
double angle = 0;
bool firing = false, enabled = true;
bool in_alarm = false;
ULONGLONG sampled_at = 0, telemetry_at = 0, controls_at = 0;
unsigned long long regular_fire_calls = 0, blocked_fire_calls = 0;
unsigned long long projectile_created_total = 0;
unsigned long long projectile_collision_total = 0, enemy_collision_total = 0;
unsigned long long enemy_destroyed_after_collision_total = 0;
double enemy_hull_damage_total = 0;
std::unordered_set<int> controlled_projectiles;
unsigned long long steps = 0, command_counter = 0;
unsigned long long scene_generation = 0;
int qa_target_enemy_id = -1;
double qa_target_enemy_hull = -999999;
std::string last_commands;
struct Controls {
    bool bot = false, aim = false, fire = false, enabled = true, qa_focus = false, qa_mutate_movement = false, qa_aim_enemy = false;
    double x = 0, y = 0, aim_x = 0, aim_y = 0;
    unsigned long long counter = 0;
    int grant = 0, screenshot = 0, kill_ally = 0, restart = 0;
} command;
double input_x = 0, input_y = 0;
bool grant_requested = false;
bool screenshot_requested = false;
bool kill_requested = false, restart_requested = false;
bool restart_in_progress = false;
bool previous_f8 = false;
bool previous_pad_start = false;

void Log(const char* text) {
    if (events) { std::fprintf(events, "[%llu tid=%lu] %s\n", GetTickCount64(), GetCurrentThreadId(), text); std::fflush(events); }
}
bool Down(int key) { return (GetAsyncKeyState(key) & 0x8000) != 0; }
double Read(nd::Instance* instance, std::uintptr_t field, double fallback = -999999, int index = nd::Scalar) {
    double value = fallback;
    return access.get(instance, field, value, index) ? value : fallback;
}
bool Write(nd::Instance* instance, std::uintptr_t field, double value, int index = nd::Scalar) {
    return access.set(instance, field, value, index);
}
int InstanceId(nd::Instance* instance) {
    // Modern runner IDs can be VALUE_REF (15). Let the runner perform the
    // conversion; the generic native numeric accessor deliberately rejects refs.
    nd::RValue raw{}; raw.kind = 5;
    const auto get = reinterpret_cast<nd::GetValue>(access.base + 0xf16590);
    const bool raw_ok = get(instance, access.id(nd::Fields::Id), nd::Scalar, &raw, false, false);
    int native_id = static_cast<int>(Read(instance, nd::Fields::Id, -1));
    if (raw_ok && (raw.kind & 0xffffff) == 15) native_id = static_cast<std::int32_t>(raw.integer);
    RValue id;
    const auto status = api->GetBuiltin("id", static_cast<CInstance*>(instance), NULL_INDEX, id);
    const int sdk_id = AurieSuccess(status) ? id.ToInt32() : -1;
    static nd::Instance* logged[4]{};
    static unsigned int logged_count = 0;
    bool seen = false;
    for (unsigned int i = 0; i < logged_count; ++i) seen = seen || logged[i] == instance;
    if (!seen && logged_count < 4) {
        logged[logged_count++] = instance;
        char text[256];
        std::snprintf(text, sizeof(text), "instance id evidence self=%p raw_ok=%d kind=%x payload=%016llx native_id=%d sdk_id=%d sdk_status=%s", instance, raw_ok, raw.kind, static_cast<unsigned long long>(raw.integer), native_id, sdk_id, AurieStatusToString(status));
        Log(text);
    }
    return sdk_id >= 100000 ? sdk_id : native_id;
}
bool Readable(std::uintptr_t address, std::size_t size) {
    if (address < 0x10000 || address > 0x00007fffffffffffULL || size > 0x1000 || address + size < address) return false;
    MEMORY_BASIC_INFORMATION region{};
    if (!VirtualQuery(reinterpret_cast<void*>(address), &region, sizeof(region))) return false;
    if (region.State != MEM_COMMIT || (region.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
    const auto end = reinterpret_cast<std::uintptr_t>(region.BaseAddress) + region.RegionSize;
    return address + size <= end;
}
bool Active(int id, nd::Instance** result = nullptr) {
    if (id < 100000) return false;
    // This exact runner's own instance resolver (native wrapper f17950) walks
    // this hash table. YYTK's room-list structure is misaligned on this build.
    // Resolve afresh on the engine thread; never retain a returned live pointer.
    const auto table = *reinterpret_cast<std::uintptr_t*>(access.base + 0x1a04228);
    const auto mask = *reinterpret_cast<std::uint32_t*>(access.base + 0x1a04230);
    const auto bucket = table + static_cast<std::uintptr_t>(mask & static_cast<std::uint32_t>(id)) * 0x10;
    if (mask > 0xffffff || (mask & (mask + 1)) != 0 || !Readable(bucket, 0x10)) {
        static unsigned int failures = 0;
        if (failures++ < 4) { char text[200]; std::snprintf(text, sizeof(text), "native instance lookup invalid table=%p mask=%x id=%d", reinterpret_cast<void*>(table), mask, id); Log(text); }
        return false;
    }
    auto node = *reinterpret_cast<std::uintptr_t*>(bucket);
    for (unsigned int visited = 0; node && visited < 1024; ++visited) {
        if (!Readable(node, 0x20)) return false;
        if (*reinterpret_cast<std::uint32_t*>(node + 0x10) == static_cast<std::uint32_t>(id)) {
            const auto instance = *reinterpret_cast<std::uintptr_t*>(node + 0x18);
            if (!Readable(instance, 0xc0) || (*reinterpret_cast<std::uint32_t*>(instance + 0xb8) & 0x100001)) return false;
            static unsigned int evidence_count = 0;
            if (evidence_count++ < 4) {
                CInstance* sdk_instance = nullptr;
                const auto sdk_status = api->GetInstanceObject(id, sdk_instance);
                char text[250];
                std::snprintf(text, sizeof(text), "native instance lookup id=%d table=%p mask=%x instance=%p sdk_instance=%p sdk_status=%s", id, reinterpret_cast<void*>(table), mask, reinterpret_cast<void*>(instance), sdk_instance, AurieStatusToString(sdk_status));
                Log(text);
            }
            if (result) *result = reinterpret_cast<nd::Instance*>(instance);
            return true;
        }
        const auto next = *reinterpret_cast<std::uintptr_t*>(node + 8);
        if (next == node) return false;
        node = next;
    }
    return false;
}
void ReadCommands() {
    const auto now = GetTickCount64();
    if (now - controls_at < 40) return;
    controls_at = now;
    std::ifstream stream(runtime_root / L"coop-control.txt");
    if (!stream) return;
    std::string contents((std::istreambuf_iterator<char>(stream)), {});
    if (contents == last_commands) return;
    last_commands = contents;
    Controls next;
    std::istringstream lines(contents);
    std::string line;
    while (std::getline(lines, line)) {
        std::replace(line.begin(), line.end(), '=', ' ');
        std::istringstream entry(line);
        std::string key; double value;
        if (!(entry >> key >> value) || !std::isfinite(value)) continue;
        if (key == "bot_x") { next.x = std::clamp(value, -1.0, 1.0); next.bot = true; }
        if (key == "bot_y") { next.y = std::clamp(value, -1.0, 1.0); next.bot = true; }
        if (key == "aim_x") { next.aim_x = value; next.aim = true; }
        if (key == "aim_y") { next.aim_y = value; next.aim = true; }
        if (key == "fire") next.fire = value != 0;
        if (key == "enabled") next.enabled = value != 0;
        if (key == "qa_focus") next.qa_focus = value != 0;
        if (key == "qa_mutate_movement") next.qa_mutate_movement = value != 0;
        if (key == "qa_aim_enemy") next.qa_aim_enemy = value != 0;
        if (key == "counter" || key == "commandcounter") next.counter = static_cast<unsigned long long>(std::max(0.0, value));
        if (key == "grant") next.grant = value != 0;
        if (key == "screenshot") next.screenshot = value != 0;
        if (key == "kill_ally") next.kill_ally = value != 0;
        if (key == "restart") next.restart = value != 0;
    }
    command = next;
    command_counter = next.counter ? next.counter : command_counter + 1;
    if (next.grant) grant_requested = true;
    if (next.screenshot) screenshot_requested = true;
    if (next.kill_ally) kill_requested = true;
    if (next.restart) restart_requested = true;
    char text[256];
    std::snprintf(text, sizeof(text), "command=%llu enabled=%d bot=%d axes=%.3f,%.3f fire=%d grant=%d", command_counter, next.enabled, next.bot, next.x, next.y, next.fire, next.grant);
    Log(text);
}
void SampleInput() {
    ReadCommands();
    const auto now = GetTickCount64();
    // Player and Ally Step often occur in the same millisecond; sample once.
    if (sampled_at == now) return;
    const double elapsed_ms = sampled_at ? static_cast<double>(std::min<ULONGLONG>(now - sampled_at, 50)) : 16.6667;
    sampled_at = now;
    enabled = command.enabled;
    input_x = (Down('L') ? 1.0 : 0) - (Down('J') ? 1.0 : 0);
    input_y = (Down('K') ? 1.0 : 0) - (Down('I') ? 1.0 : 0);
    firing = Down('N');
    bool aim_input = false;
    XINPUT_STATE pad{};
    bool pad_start = false;
    // XInput slot one is the second controller; slot zero remains untouched.
    if (XInputGetState(1, &pad) == ERROR_SUCCESS) {
        const double x = pad.Gamepad.sThumbLX / 32767.0;
        const double y = -pad.Gamepad.sThumbLY / 32767.0;
        if (std::hypot(x, y) > 0.24) { input_x = x; input_y = y; }
        const double ax = pad.Gamepad.sThumbRX / 32767.0;
        const double ay = -pad.Gamepad.sThumbRY / 32767.0;
        if (std::hypot(ax, ay) > 0.24) { angle = std::atan2(-ay, ax) * 180.0 / 3.141592653589793; aim_input = true; }
        firing = firing || pad.Gamepad.bRightTrigger > 30 || (pad.Gamepad.wButtons & XINPUT_GAMEPAD_A);
        pad_start = (pad.Gamepad.wButtons & XINPUT_GAMEPAD_START) != 0;
    }
    if (pad_start && !previous_pad_start) grant_requested = true;
    previous_pad_start = pad_start;
    if (command.bot) { input_x = command.x; input_y = command.y; firing = command.fire; }
    if (command.aim && std::hypot(command.aim_x, command.aim_y) > 0.001) {
        angle = std::atan2(-command.aim_y, command.aim_x) * 180.0 / 3.141592653589793;
        aim_input = true;
    }
    if (Down('U')) { angle += 0.24 * elapsed_ms; aim_input = true; }
    if (Down('O')) { angle -= 0.24 * elapsed_ms; aim_input = true; }
    const double length = std::hypot(input_x, input_y);
    if (length > 1) { input_x /= length; input_y /= length; }
    if (!aim_input && length > 0.001) angle = std::atan2(-input_y, input_x) * 180.0 / 3.141592653589793;
    angle = std::fmod(angle + 720.0, 360.0);
    const bool f8 = Down(VK_F8);
    if (f8 && !previous_f8) grant_requested = true;
    previous_f8 = f8;
}

void Telemetry(nd::Instance* player = nullptr) {
    const auto now = GetTickCount64();
    if (now - telemetry_at < 100 || !telemetry) return;
    telemetry_at = now;
    nd::Instance* ally = nullptr;
    const bool alive = Active(selected_id, &ally);
    if (player == nullptr) {
        const auto object = api->CallBuiltin("asset_get_index", {YYTK::RValue("oPlayer")});
        if (object.ToInt32() >= 0) {
            const auto id = api->CallBuiltin("instance_find", {object, YYTK::RValue(0)}).ToInt32();
            Active(id, &player);
        }
    }
    const int ally_count = api->CallBuiltin("instance_number", {YYTK::RValue(362)}).ToInt32();
    const int player_count = api->CallBuiltin("instance_number", {YYTK::RValue(603)}).ToInt32();
    const int player_id = player ? InstanceId(player) : -1;
    std::fprintf(telemetry,
        "{\"time_ms\":%llu,\"command\":%llu,\"scene_generation\":%llu,\"enabled\":%d,\"qa_mutate_movement\":%d,\"qa_aim_enemy\":%d,\"qa_target_enemy_id\":%d,\"qa_target_enemy_hull\":%.5f,\"ally_id\":%d,\"alive\":%d,\"ally_count\":%d,\"player_count\":%d,\"x\":%.5f,\"y\":%.5f,\"hull\":%.5f,\"alarm0\":%.5f,\"angle\":%.5f,\"fire\":%d,\"regular_fire_calls\":%llu,\"blocked_fire_calls\":%llu,\"projectile_created_total\":%llu,\"projectile_collision_total\":%llu,\"enemy_collision_total\":%llu,\"enemy_hull_damage_total\":%.5f,\"enemy_destroyed_after_collision_total\":%llu,\"steps\":%llu,\"player_id\":%d,\"player_alive\":%d,\"player_hull\":%.5f,\"player_x\":%.5f,\"player_y\":%.5f}\n",
        now, command_counter, scene_generation, enabled, command.qa_mutate_movement, command.qa_aim_enemy, qa_target_enemy_id, qa_target_enemy_hull, selected_id, alive, ally_count, player_count,
        ally ? Read(ally, nd::Fields::X) : -999999, ally ? Read(ally, nd::Fields::Y) : -999999,
        ally ? Read(ally, nd::Fields::Hull) : -999999, ally ? Read(ally, nd::Fields::Alarm, -999999, 0) : -999999,
        angle, firing, regular_fire_calls, blocked_fire_calls, projectile_created_total,
        projectile_collision_total, enemy_collision_total, enemy_hull_damage_total, enemy_destroyed_after_collision_total,
        steps, player_id, player_id >= 0,
        player ? Read(player, nd::Fields::Hull) : -999999,
        player ? Read(player, nd::Fields::X) : -999999, player ? Read(player, nd::Fields::Y) : -999999);
    std::fflush(telemetry);
}

void AimAtExistingEnemy(nd::Instance* self) {
    qa_target_enemy_id = -1;
    qa_target_enemy_hull = -999999;
    if (!command.qa_aim_enemy) return;
    const double x = Read(self, nd::Fields::X), y = Read(self, nd::Fields::Y);
    if (x == -999999 || y == -999999) return;
    double best_distance_squared = 1e100;
    for (int object : {483, 478, 474}) {
        const int id = api->CallBuiltin("instance_nearest", {RValue(x), RValue(y), RValue(object)}).ToInt32();
        nd::Instance* target = nullptr;
        if (!Active(id, &target)) continue;
        const double hull = Read(target, nd::Fields::Hull);
        if (hull != -999999 && hull <= 0) continue;
        const double tx = Read(target, nd::Fields::X), ty = Read(target, nd::Fields::Y);
        if (tx == -999999 || ty == -999999) continue;
        const double dx = tx - x, dy = ty - y;
        const double distance_squared = dx * dx + dy * dy;
        if (distance_squared >= best_distance_squared || distance_squared < 0.000001) continue;
        best_distance_squared = distance_squared;
        qa_target_enemy_id = id;
        qa_target_enemy_hull = hull;
        angle = std::fmod(std::atan2(-dy, dx) * 180.0 / 3.141592653589793 + 360.0, 360.0);
    }
}
void Alarm0(nd::Instance* self, nd::Instance* other) {
    if (!enabled || self != selected) { original_alarm0(self, other); return; }
    if (!firing || in_alarm) { ++blocked_fire_calls; return; }
    in_alarm = true;
    AimAtExistingEnemy(self);
    Write(self, nd::Fields::FireRegular, 1);
    Write(self, nd::Fields::Angle, angle);
    original_alarm0(self, other);
    ++regular_fire_calls; // An emitter event call, not a claimed projectile/hit count.
    in_alarm = false;
}
void Alarm2(nd::Instance* self, nd::Instance* other) {
    if (enabled && self == selected) return; // Prototype uses the Ally's own regular weapon.
    original_alarm2(self, other);
}
void ProjectileCreate(nd::Instance* self, nd::Instance* other) {
    original_projectile_create(self, other);
    // Count actual advanced drone bullet Create events during the selected Ally's
    // regular weapon emitter. Unrelated AI/player projectile creates are ignored.
    if (in_alarm) {
        ++projectile_created_total;
        const int id = InstanceId(self);
        if (id >= 100000) {
            if (controlled_projectiles.size() >= 65536) { controlled_projectiles.clear(); Log("projectile provenance retention limit reached; older IDs discarded"); }
            controlled_projectiles.insert(id);
        }
    }
}
void ProjectileCollision(nd::Instance* self, nd::Instance* other) {
    const int projectile_id = InstanceId(self);
    if (!controlled_projectiles.contains(projectile_id) || !other) {
        original_projectile_collision(self, other);
        return;
    }
    const int target_id = InstanceId(other);
    const double before = Read(other, nd::Fields::Hull);
    RValue object;
    int object_index = -1;
    if (AurieSuccess(api->GetBuiltin("object_index", static_cast<CInstance*>(other), NULL_INDEX, object))) object_index = object.ToInt32();
    bool enemy = false;
    // Verified shipped parent objects: EnemyStandard483, EnemySegment478,
    // EnemyHardShield474 all derive from Solid742. Retain the actual name too.
    if (object_index >= 0) {
        for (int parent : {483, 478, 474}) {
            enemy = enemy || object_index == parent || api->CallBuiltin("object_is_ancestor", {RValue(object_index), RValue(parent)}).ToBoolean();
        }
    }
    const std::string object_name = object_index >= 0 ? api->CallBuiltin("object_get_name", {RValue(object_index)}).ToString() : "unknown";
    original_projectile_collision(self, other);
    ++projectile_collision_total;
    if (enemy) ++enemy_collision_total;
    nd::Instance* target = nullptr;
    const bool target_alive = Active(target_id, &target);
    const double after = target_alive ? Read(target, nd::Fields::Hull) : -999999;
    char text[512];
    std::snprintf(text, sizeof(text), "controlled projectile collision projectile_id=%d target_id=%d object=%d name=%s enemy=%d hull_before=%.5f hull_after=%.5f target_alive=%d", projectile_id, target_id, object_index, object_name.c_str(), enemy, before, after, target_alive);
    Log(text);
}
void SolidDroneCollision(nd::Instance* self, nd::Instance* other) {
    // Collision dispatch invokes the target's event before the bullet's event.
    // Wrap the event which calls droneProjectileHitsSolid, so HP is sampled on
    // either side of the actual damage code, with the attacking bullet proven.
    if (!other || !controlled_projectiles.contains(InstanceId(other))) {
        original_solid_drone_collision(self, other);
        return;
    }
    const int projectile_id = InstanceId(other);
    const int target_id = InstanceId(self);
    const double before = Read(self, nd::Fields::Hull);
    RValue object;
    int object_index = -1;
    if (AurieSuccess(api->GetBuiltin("object_index", static_cast<CInstance*>(self), NULL_INDEX, object))) object_index = object.ToInt32();
    bool enemy = false;
    if (object_index >= 0) {
        for (int parent : {483, 478, 474}) enemy = enemy || object_index == parent || api->CallBuiltin("object_is_ancestor", {RValue(object_index), RValue(parent)}).ToBoolean();
    }
    original_solid_drone_collision(self, other);
    nd::Instance* target = nullptr;
    const bool target_alive = Active(target_id, &target);
    const double after = target_alive ? Read(target, nd::Fields::Hull) : -999999;
    if (enemy && before > 0 && after != -999999 && after < before) {
        // HP below zero is overkill. Count actual remaining hull removed only.
        enemy_hull_damage_total += before - std::max(0.0, after);
    }
    if (enemy && !target_alive) ++enemy_destroyed_after_collision_total;
    char text[512];
    std::snprintf(text, sizeof(text), "target drone collision damage evidence projectile_id=%d target_id=%d object=%d enemy=%d hull_before=%.5f hull_after=%.5f target_alive=%d", projectile_id, target_id, object_index, enemy, before, after, target_alive);
    Log(text);
}
void AllyStep(nd::Instance* self, nd::Instance* other) {
    SampleInput();
    const int id = InstanceId(self);
    if (selected_id < 0 || !Active(selected_id)) {
        selected_id = id;
        selected = self;
        char text[100]; std::snprintf(text, sizeof(text), "selected ally id=%d pointer=%p", id, self); Log(text);
    }
    const bool controlled = id == selected_id;
    if (controlled) selected = self;
    if (controlled && enabled) AimAtExistingEnemy(self);
    original_step(self, other);
    // The original event can destroy the Ally when its owner dies.
    nd::Instance* live = nullptr;
    if (!controlled || !Active(id, &live)) { if (controlled) { selected = nullptr; selected_id = -1; } return; }
    ++steps;
    if (enabled) {
        AimAtExistingEnemy(live);
        const double maximum = std::clamp(Read(live, nd::Fields::MoveSpeed, 6), 1.0, 20.0);
        if (!command.qa_mutate_movement) {
            Write(live, nd::Fields::HSpeed, input_x * maximum);
            Write(live, nd::Fields::VSpeed, input_y * maximum);
        }
        Write(live, nd::Fields::Angle, angle);
        Write(live, nd::Fields::FireRegular, firing ? 1 : 0);
        Write(live, nd::Fields::FireIntegrated, 0);
        Write(live, nd::Fields::CanFireIntegrated, 0);
        if (firing && Read(live, nd::Fields::Alarm, 1, 0) <= 0) Alarm0(live, other);
    }
    Telemetry();
}

void GrantAlly(nd::Instance* player) {
    // Reproduces the natural zero-argument native call in Player Step. The script
    // spawns oConstructAllyStandard (363), initializes it from this live player,
    // and performs its user event. Directly creating parent object 362 is unsafe.
    if (Active(selected_id)) { Log("grant skipped: controlled Ally is already alive"); return; }
    nd::RValue result{};
    result.kind = 5;
    const auto deploy = reinterpret_cast<nd::Script>(access.base + 0x17a080);
    deploy(player, player, &result, 0, nullptr);
    Log("deployAlly native script called with live player and zero arguments");
}
void ClearSelection() { selected = nullptr; selected_id = -1; }
void ResetSceneTracking() {
    ClearSelection();
    controlled_projectiles.clear();
    qa_target_enemy_id = -1;
    qa_target_enemy_hull = -999999;
    in_alarm = false;
    grant_requested = false;
    kill_requested = false;
    screenshot_requested = false;
}
void PlayerCreate(nd::Instance* self, nd::Instance* other) {
    original_player_create(self, other);
    ResetSceneTracking();
    ++scene_generation;
    char text[160]; std::snprintf(text, sizeof(text), "Player Create completed; scene_generation=%llu selection/provenance reset", scene_generation); Log(text);
}
nd::RValue* RestartGame(nd::Instance* self, nd::Instance* other, nd::RValue* result, int argc, nd::RValue** args) {
    char text[250];
    std::snprintf(text, sizeof(text), "native restartGame invocation pointer=%p self=%p other=%p argc=%d; game cleanup preserved", reinterpret_cast<void*>(access.base + 0x4f1e90), self, other, argc);
    Log(text);
    ResetSceneTracking();
    return original_restart_game(self, other, result, argc, args);
}
void SaveRequestedScreenshot() {
    if (!screenshot_requested) return;
    screenshot_requested = false;
    const auto screenshot = "qa-screen-" + std::to_string(command_counter) + ".png";
    api->CallBuiltin("screen_save", {YYTK::RValue(screenshot)});
    Log("screen_save requested in game_save_id; verify actual PNG before treating as captured");
}
void PlayerStep(nd::Instance* self, nd::Instance* other) {
    const int id = InstanceId(self);
    original_player_step(self, other);
    SampleInput();
    nd::Instance* resolved = nullptr;
    if (!Active(id, &resolved)) return;
    static bool pointer_verified = false;
    if (!pointer_verified) {
        char text[200]; std::snprintf(text, sizeof(text), "native PlayerStep pointer crosscheck id=%d self=%p resolved=%p match=%d", id, self, resolved, self == resolved); Log(text);
        pointer_verified = true;
    }
    if (resolved != self) { Log("REFUSING PlayerStep control: native instance resolver disagrees with self"); return; }
    if (selected_id >= 100000 && !Active(selected_id)) { ClearSelection(); Log("controlled Ally inactive; selection cleared"); }
    if (grant_requested) { grant_requested = false; GrantAlly(self); }
    SaveRequestedScreenshot();
    Telemetry(self);
}

bool Signature(std::uintptr_t rva, const unsigned char (&bytes)[16]) {
    return std::memcmp(reinterpret_cast<void*>(access.base + rva), bytes, 16) == 0;
}
bool CheckBuild() {
    const unsigned char step[16] = {0x48,0x89,0x5c,0x24,0x18,0x48,0x89,0x54,0x24,0x10,0x48,0x89,0x4c,0x24,0x08,0x55};
    const unsigned char alarm0[16] = {0x48,0x8b,0xc4,0x48,0x89,0x58,0x20,0x48,0x89,0x50,0x10,0x48,0x89,0x48,0x08,0x55};
    const unsigned char alarm2[16] = {0x48,0x8b,0xc4,0x48,0x89,0x58,0x10,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56};
    const unsigned char getter[16] = {0x40,0x55,0x56,0x41,0x55,0x41,0x56,0x48,0x83,0xec,0x38,0x48,0x63,0xf2,0x4d,0x8b};
    const unsigned char setter[16] = {0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x48};
    const unsigned char player[16] = {0x48,0x89,0x54,0x24,0x10,0x48,0x89,0x4c,0x24,0x08,0x55,0x53,0x56,0x57,0x41,0x54};
    const unsigned char projectile[16] = {0x48,0x8b,0xc4,0x48,0x89,0x58,0x08,0x48,0x89,0x70,0x10,0x48,0x89,0x78,0x18,0x4c};
    const unsigned char player_create[16] = {0x48,0x8b,0xc4,0x48,0x89,0x58,0x18,0x48,0x89,0x50,0x10,0x55,0x56,0x57,0x41,0x54};
    const unsigned char solid_drone_collision[16] = {0x40,0x53,0x48,0x83,0xec,0x60,0x48,0x8d,0x05,0x23,0x7a,0x99,0x00,0x48,0x89,0x44};
    const unsigned char restart_game[16] = {0x48,0x8b,0xc4,0x48,0x89,0x58,0x18,0x48,0x89,0x50,0x10,0x48,0x89,0x48,0x08,0x55};
    // deployAlly shares this short prologue with Alarm0; its RVA and the player's
    // callsite were established separately from the pristine pinned artifact.
    return Signature(0x91cbc0, step) && Signature(0x917b30, alarm0) && Signature(0x9158c0, alarm2) && Signature(0xf16590, getter) && Signature(0xf17460, setter) && Signature(0xc4a650, player) && Signature(0xc41ce0, player_create) && Signature(0x17a080, alarm0) && Signature(0xa50980, projectile) && Signature(0xa52710, alarm0) && Signature(0xd86520, solid_drone_collision) && Signature(0x4f1e90, restart_game);
}
bool Hook(AurieModule* module, const char* name, std::uintptr_t rva, nd::Event detour, nd::Event& original) {
    const auto status = MmCreateHook(module, name, reinterpret_cast<void*>(access.base + rva), reinterpret_cast<void*>(detour), reinterpret_cast<void**>(&original));
    char text[256]; std::snprintf(text, sizeof(text), "hook %s RVA=%llx status=%s", name, static_cast<unsigned long long>(rva), AurieStatusToString(status)); Log(text);
    return AurieSuccess(status);
}
void FilterPad(TRoutine original, RValue& result, CInstance* self, CInstance* other, int argc, RValue* args) {
    if (enabled && argc > 0 && args && args[0].ToInt32() == 1) { result = RValue(0.0); return; }
    original(result, self, other, argc, args);
}
void PadConnected(RValue& r, CInstance* s, CInstance* o, int n, RValue* a) { FilterPad(original_connected, r, s, o, n, a); }
void PadAxis(RValue& r, CInstance* s, CInstance* o, int n, RValue* a) { FilterPad(original_axis, r, s, o, n, a); }
void PadButton(RValue& r, CInstance* s, CInstance* o, int n, RValue* a) { FilterPad(original_button, r, s, o, n, a); }
void PadPressed(RValue& r, CInstance* s, CInstance* o, int n, RValue* a) { FilterPad(original_pressed, r, s, o, n, a); }
void PadReleased(RValue& r, CInstance* s, CInstance* o, int n, RValue* a) { FilterPad(original_released, r, s, o, n, a); }
void PadButtonValue(RValue& r, CInstance* s, CInstance* o, int n, RValue* a) { FilterPad(original_button_value, r, s, o, n, a); }
void WindowFocus(RValue& r, CInstance* s, CInstance* o, int n, RValue* a) {
    if (command.qa_focus) { r = RValue(true); return; }
    original_focus(r, s, o, n, a);
}
void CodeCallback(NativeCodeEvent& context) {
    SampleInput();
    if (restart_requested && !restart_in_progress) {
        restart_requested = false; // Clear before the engine call can dispatch events.
        restart_in_progress = true;
        // Finish the current event before native cleanup can destroy its self.
        context.Call();
        const int active_logs = api->CallBuiltin("instance_number", {RValue(573)}).ToInt32();
        const int active_players = api->CallBuiltin("instance_number", {RValue(603)}).ToInt32();
        char guard[180];
        std::snprintf(guard, sizeof(guard), "QA restart guard active_log=%d active_player=%d", active_logs, active_players);
        Log(guard);
        // Native destroyDataStructures assumes gameplay/log instances were
        // deactivated by the real pause flow. Active oLog would destroy its grid
        // twice (explicit script cleanup followed by the object's Cleanup event).
        if (active_logs != 0) {
            Log("QA restart refused: pause through the game's menu before requesting restart");
            restart_in_progress = false;
            return;
        }
        ResetSceneTracking();
        Log("QA restartGame requested; creating native oGameRestart504 after current event");
        // Native oGameRestart Create calls restartGame with zero arguments and
        // its own live instance as self. The script clears pools/data structures,
        // unlike the engine's bare game_restart builtin.
        api->CallBuiltin("instance_create_depth", {RValue(0.0), RValue(0.0), RValue(0.0), RValue(504)});
        restart_in_progress = false;
        return;
    }
    if (kill_requested) {
        kill_requested = false;
        nd::Instance* ally = nullptr;
        if (Active(selected_id, &ally)) {
            const bool written = Write(ally, nd::Fields::Hull, 0);
            Log(written ? "QA-only kill_ally stimulus set hull=0; waiting for natural destruction" : "QA-only kill_ally hull write failed");
        } else Log("QA-only kill_ally skipped: no active controlled Ally");
    }
    if (command.qa_focus && !restart_in_progress) {
        // Report fresh active-instance state in menus/pauses too. This does not
        // advance the native Ally Step counter or imply that gameplay progressed.
        Telemetry();
        SaveRequestedScreenshot();
    }
}
bool HookBuiltin(AurieModule* module, const char* name, TRoutine detour, TRoutine& original) {
    void* target = nullptr;
    auto status = api->GetNamedRoutinePointer(name, &target);
    if (AurieSuccess(status) && target) status = MmCreateHook(module, name, target, reinterpret_cast<void*>(detour), reinterpret_cast<void**>(&original));
    char text[256]; std::snprintf(text, sizeof(text), "controller isolation hook %s target=%p status=%s", name, target, AurieStatusToString(status)); Log(text);
    return AurieSuccess(status) && original;
}
}

EXPORTED AurieStatus ModuleInitialize(AurieModule* module, const fs::path& path) {
    runtime_root = path.parent_path().parent_path().parent_path();
    _wfopen_s(&events, (runtime_root / L"coop-events.log").c_str(), L"a");
    _wfopen_s(&telemetry, (runtime_root / L"coop-telemetry.jsonl").c_str(), L"a");
    Log("local Ally prototype initialize SDK=Zeus d5cc0078; regular-fire calls are not projectile/hit assertions");
    api = GetInterface();
    if (!api) return AURIE_MODULE_DEPENDENCY_NOT_RESOLVED;
    short major, minor, patch; api->QueryVersion(major, minor, patch);
    if (major != 5) return AURIE_MODULE_DEPENDENCY_NOT_RESOLVED;
    access.base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    if (!CheckBuild()) { Log("REFUSING: native code signatures differ from pinned game"); return AURIE_MODULE_INITIALIZATION_FAILED; }
    if (!Hook(module, "NovaCoop.Alarm0", 0x917b30, Alarm0, original_alarm0) ||
        !Hook(module, "NovaCoop.Alarm2", 0x9158c0, Alarm2, original_alarm2) ||
        !Hook(module, "NovaCoop.ProjectileCreate", 0xa50980, ProjectileCreate, original_projectile_create) ||
        !Hook(module, "NovaCoop.ProjectileCollision", 0xa52710, ProjectileCollision, original_projectile_collision) ||
        !Hook(module, "NovaCoop.SolidDroneCollision", 0xd86520, SolidDroneCollision, original_solid_drone_collision) ||
        !Hook(module, "NovaCoop.AllyStep", 0x91cbc0, AllyStep, original_step) ||
        !Hook(module, "NovaCoop.PlayerCreate", 0xc41ce0, PlayerCreate, original_player_create) ||
        !Hook(module, "NovaCoop.PlayerStep", 0xc4a650, PlayerStep, original_player_step)) return AURIE_MODULE_INITIALIZATION_FAILED;
    const auto restart_status = MmCreateHook(module, "NovaCoop.RestartGame", reinterpret_cast<void*>(access.base + 0x4f1e90), reinterpret_cast<void*>(RestartGame), reinterpret_cast<void**>(&original_restart_game));
    if (!AurieSuccess(restart_status)) return restart_status;
    Log("native restartGame hook RVA=4f1e90 installed; QA restart uses own oGameRestart Create context");
    if (!HookBuiltin(module, "gamepad_is_connected", PadConnected, original_connected) ||
        !HookBuiltin(module, "gamepad_axis_value", PadAxis, original_axis) ||
        !HookBuiltin(module, "gamepad_button_check", PadButton, original_button) ||
        !HookBuiltin(module, "gamepad_button_check_pressed", PadPressed, original_pressed) ||
        !HookBuiltin(module, "gamepad_button_check_released", PadReleased, original_released) ||
        !HookBuiltin(module, "gamepad_button_value", PadButtonValue, original_button_value) ||
        !HookBuiltin(module, "window_has_focus", WindowFocus, original_focus)) return AURIE_MODULE_INITIALIZATION_FAILED;
    const auto callback = api->CreateCallback(module, EVENT_OBJECT_CALL, reinterpret_cast<void*>(CodeCallback), 0);
    if (!AurieSuccess(callback)) return callback;
    return AURIE_SUCCESS;
}
