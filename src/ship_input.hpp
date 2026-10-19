#pragma once
// Exact-build input map for NovaDrift.exe SHA256 6555f9f49c9b44f2a38dafc0846dd8af0e7e1f1e100cba976db60967b7173e2d.
// This header never changes the process global object or installs hooks. Callers
// expose Overlay::Find() only inside owner 2 and enter that scope BEFORE Step.
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "native_abi.hpp"
#include "ship_config.hpp"

namespace ndship { namespace input {
namespace native = novadrift_native;
inline constexpr std::uintptr_t GlobalObjectCell = 0x1b1c8d0;
inline constexpr std::uintptr_t InputHeld = 0x39a000;
inline constexpr std::uintptr_t InputPressed = 0x39ab60;
inline constexpr std::uintptr_t InputReleased = 0x39b6d0;
inline constexpr std::uintptr_t GamepadHeld = 0x302380;
inline constexpr std::uintptr_t GamepadPressed = 0x3038d0;
inline constexpr std::uintptr_t GamepadAxisValue = 0x301ea0;
inline constexpr std::uintptr_t RawKeyboardHeld = 0xf85b50;
inline constexpr int SoftwareCursorAsset = 741;
inline constexpr int KeyThrust = 'I', KeyLeft = 'J', KeyRight = 'L', KeyFire = 'N';
inline constexpr int KeyDeploy = 'U', KeyStabilize = 'K', KeyFormation = 'O';
// Win32 virtual keys, deliberately kept numeric so this header needs no SDK.
inline constexpr int KeyBlink = 0xa3; // right Ctrl
inline constexpr int KeyConstruct1 = '7', KeyConstruct2 = '8', KeyConstruct3 = '9';
inline constexpr int KeyConstruct4 = '0', KeyConstruct5 = 0xbd; // OEM minus
inline constexpr int KeyStrafe = 0xa5; // right Alt
inline constexpr int KeyStrafeLeft = 0xba, KeyStrafeRight = 0xde; // ; and '
inline constexpr int KeyStrafeForward = 0xdb, KeyStrafeBack = 0xdd; // [ and ]
inline constexpr std::uintptr_t SelectedConstructCell = 0x1aee7e8;

struct FieldDefault { const char* name; std::uintptr_t cell_rva; double value; };
// Values are configured key/device IDs, not live key state. -1 disables an
// unassigned binding. Gamepad builtins also need owner-2 suppression below;
// gamepadHeld otherwise polls P1 hardware and writes gamepadInUse through a
// pointer returned from the global read slot.
inline constexpr FieldDefault Defaults[] = {
    {"fireKey",0x1afc788,KeyFire}, {"fireKey2",0x1afc598,-1},
    {"thrustKey",0x1afc7e8,KeyThrust}, {"thrustKey2",0x1afc6e8,-1},
    {"leftKey",0x1afc678,KeyLeft}, {"rightKey",0x1afc688,KeyRight},
    // Up/down also select native blink/strafe direction. Down is the game's
    // keyboard stabilization binding; it is not a separate stabilizeKey.
    {"upKey",0x1afc708,KeyThrust}, {"downKey",0x1afc608,KeyStabilize},
    {"directionalControlScheme",0x1b000a8,0}, {"thrustControlScheme",0x1b00178,0},
    {"player1",0x1afc718,-1}, {"gamepadInUse",0x1b01038,0},
    {"gamepadAxisHorizontal",0x1af1f78,0}, {"gamepadAxisVertical",0x1af20b8,1},
    {"gamepadFire",0x1afc418,-1}, {"gamepadThrust",0x1afc4e8,-1},
    {"toggleAutoFiring",0x1b002e8,0}, {"autoFiringOn",0x1b00348,0},
    {"construct1Key",0x1afc778,KeyConstruct1}, {"construct2Key",0x1afc638,KeyConstruct2},
    {"construct3Key",0x1afc758,KeyConstruct3}, {"construct4Key",0x1afc6f8,KeyConstruct4},
    {"construct5Key",0x1afc7c8,KeyConstruct5}, {"gamepadDeployConstruct",0x1afc458,-1},
    {"strafeLeftKey",0x1afc728,KeyStrafeLeft}, {"gamepadStrafeLeft",0x1afc4b8,-1},
    {"strafeRightKey",0x1afc5b8,KeyStrafeRight}, {"gamepadStrafeRight",0x1afc3d8,-1},
    {"strafeForwardKey",0x1afc6a8,KeyStrafeForward}, {"gamepadStrafeForward",0x1afc408,-1},
    {"strafeBackKey",0x1afc5c8,KeyStrafeBack}, {"gamepadStrafeBack",0x1afc438,-1},
    {"strafeKey",0x1afc648,KeyStrafe}, {"gamepadStrafe",0x1afc488,-1},
    {"blinkKey",0x1afc568,KeyBlink}, {"gamepadBlink",0x1afc528,-1},
    {"gamepadStabilize",0x1afc478,-1}, {"formationKey",0x1afc738,KeyFormation},
    {"gamepadFormation",0x1afc4d8,-1},
    {"gamepadToggleConstructLeft",0x1afc3e8,-1},
    {"gamepadToggleConstructRight",0x1afc378,-1},
    {"stressTestFullAuto",0x1af1f08,0},
    // Shared cursor lifetime is NOT changed by this setting. The native Player
    // Step instance_exists(741) branch must be hidden separately for owner 2.
    {"disableMouseAim",0x1b00068,1},
};
inline constexpr std::size_t FieldCount = sizeof(Defaults) / sizeof(Defaults[0]);

inline native::RValue Number(double number) noexcept {
    native::RValue value{}; value.real = number; value.kind = 0; return value;
}
// Callback contract: copy the numeric value into stable per-owner storage.
// Never save the address of this callback's temporary RValue.
template<class SetOverlay>
inline void ConfigureInputOverlay(SetOverlay&& set) {
    for (const auto& field : Defaults) set(field.cell_rva, Number(field.value));
}

class Overlay {
    std::array<int, FieldCount> ids_{};
    std::array<native::RValue, FieldCount> values_{};
    bool initialized_ = false;
public:
    Overlay() noexcept { ResetValues(); ids_.fill(-1); }
    void ResetValues() noexcept {
        for (std::size_t i = 0; i < FieldCount; ++i) values_[i] = Number(Defaults[i].value);
    }
    // Call after runner has initialized the .data variable-ID cells. Returns
    // false without becoming active if any ID is unresolved. Heap location of
    // this object must remain stable while original native code caches slots.
    bool Initialize(std::uintptr_t module_base) noexcept {
        initialized_ = false;
        if (!module_base) return false;
        for (std::size_t i = 0; i < FieldCount; ++i) {
            ids_[i] = *reinterpret_cast<const int*>(module_base + Defaults[i].cell_rva);
            if (ids_[i] < 0) return false;
        }
        ResetValues(); initialized_ = true; return true;
    }
    native::RValue* Find(int runtime_field_id) noexcept {
        if (!initialized_) return nullptr;
        for (std::size_t i = 0; i < FieldCount; ++i)
            if (ids_[i] == runtime_field_id) return &values_[i];
        return nullptr;
    }
    bool Initialized() const noexcept { return initialized_; }
};

enum Button : unsigned {
    Thrust = 1u, TurnLeft = 2u, TurnRight = 4u, Fire = 8u,
    Blink = 1u << 4, Stabilize = 1u << 5, Formation = 1u << 6,
    Construct1 = 1u << 7, Construct2 = 1u << 8, Construct3 = 1u << 9,
    Construct4 = 1u << 10, Construct5 = 1u << 11,
    StrafeLeft = 1u << 12, StrafeRight = 1u << 13,
    StrafeForward = 1u << 14, StrafeBack = 1u << 15, Strafe = 1u << 16,
    DeploySelected = 1u << 17
};
inline constexpr unsigned AllButtons = (1u << 18) - 1u;
struct KeyBinding { int key; unsigned button; };
inline constexpr KeyBinding Bindings[] = {
    {KeyThrust,Thrust}, {KeyLeft,TurnLeft}, {KeyRight,TurnRight}, {KeyFire,Fire},
    {KeyBlink,Blink}, {KeyStabilize,Stabilize}, {KeyFormation,Formation},
    {KeyConstruct1,Construct1}, {KeyConstruct2,Construct2}, {KeyConstruct3,Construct3},
    {KeyConstruct4,Construct4}, {KeyConstruct5,Construct5},
    {KeyStrafeLeft,StrafeLeft}, {KeyStrafeRight,StrafeRight},
    {KeyStrafeForward,StrafeForward}, {KeyStrafeBack,StrafeBack}, {KeyStrafe,Strafe},
    {KeyDeploy,DeploySelected}
};
enum class Edge { Held, Pressed, Released };
inline unsigned KeyMask(int key) noexcept {
    for (const auto& binding : Bindings) if (binding.key == key) return binding.button;
    return 0;
}
inline bool Numeric(const native::RValue* value, double& result) noexcept {
    if (!value) return false;
    switch (value->kind & 0xffffffu) {
        case 0: case 13: result = value->real; return std::isfinite(result);
        case 7: result = static_cast<std::int32_t>(value->integer); return true;
        case 10: result = static_cast<double>(value->integer); return true;
        default: return false;
    }
}
// Parent polls only the selected physical XInput slot. x/y are normalized world
// directions (positive x right, positive y DOWN); convert XInput thumb Y's sign.
// Trigger masks use config::LeftTrigger / RightTrigger after a threshold check.
struct ControllerFrame {
    bool connected = false;
    double x = 0, y = 0;
    std::uint32_t buttons = 0;
};
inline constexpr unsigned ActionButtons[config::ActionCount] = {
    0,0,0,0,Fire,Blink,Stabilize,Formation,DeploySelected,
    Construct1,Construct2,Construct3,Construct4,Construct5,
    StrafeLeft,StrafeRight,StrafeForward,StrafeBack,Strafe
};
// Physical bindings are translated into FIXED native helper tokens above.
// The game therefore retains its tank mode, native thrust/turn acceleration,
// collisions and inertia while the user may choose convenient directional keys.
// QA's semantic SampleBot bypasses this entire physical profile translation.
template<class IsDown>
inline unsigned ConfiguredButtons(const config::Profile& profile, IsDown&& down,
                                  double native_angle, const ControllerFrame& pad = {}, bool forward_only = false) noexcept {
    const bool keyboard = profile.device != config::Device::Controller;
    const bool controller = profile.device != config::Device::Keyboard && pad.connected;
    bool actions[config::ActionCount]{};
    for (std::size_t i = 0; i < config::ActionCount; ++i)
        actions[i] = (keyboard && profile.keys[i] > 0 && down(profile.keys[i]))
            || (controller && profile.pad[i] && (pad.buttons & profile.pad[i]) != 0);
    unsigned buttons = 0;
    for (std::size_t i = 4; i < config::ActionCount; ++i) if (actions[i]) buttons |= ActionButtons[i];
    double x = (actions[3]?1.0:0.0) - (actions[1]?1.0:0.0);
    double y = (actions[2]?1.0:0.0) - (actions[0]?1.0:0.0);
    double px = controller && std::isfinite(pad.x) ? pad.x : 0;
    double py = controller && std::isfinite(pad.y) ? pad.y : 0;
    const double radius = std::sqrt(px*px + py*py);
    if (radius > profile.deadzone && radius > 0) {
        const double strength = std::fmin(1.0,(radius-profile.deadzone)/(1.0-profile.deadzone));
        px = px/radius*strength; py = py/radius*strength;
    } else px = py = 0;
    x += px; y += py;
    if (profile.steering == config::Steering::Tank) {
        if (x < -0.05) buttons |= TurnLeft;
        if (x > 0.05) buttons |= TurnRight;
        if(forward_only){
            // P2 flies with thrust and two rotation buttons. Down remains
            // available to menus; braking uses its separate Stabilize action.
            if(actions[0] || py < -0.05)buttons |= Thrust;
        }else{
            if (y < -0.05) buttons |= Thrust;
            if (y > 0.05) buttons |= Stabilize;
        }
    } else if (x*x+y*y > 0.0025 && std::isfinite(native_angle)) {
        const double desired = std::atan2(-y,x) * (180.0/3.14159265358979323846);
        const double error = std::remainder(desired-native_angle,360.0);
        // GameMaker angles increase anticlockwise; native J turns left. Stop
        // commanding a turn within four degrees, allowing original damping.
        if (error > 4.0) buttons |= TurnLeft;
        if (error < -4.0) buttons |= TurnRight;
        // Turn toward a requested heading before accelerating into it. Native
        // ship speed is never overwritten, including when the keys are released.
        if (std::fabs(error) <= 80.0) buttons |= Thrust;
    }
    return buttons;
}
// Native menus use fixed helper tokens I/J/O/L and N/K/U/8. Map the profile's
// logical movement/Fire/Stabilize/Deploy/Construct2 controls into those tokens;
// physical K therefore navigates DOWN, never declines a choice by accident.
template<class IsDown>
inline unsigned ConfiguredMenuButtons(const config::Profile& profile, IsDown&& down,
                                     const ControllerFrame& pad = {}) noexcept {
    const bool keyboard = profile.device != config::Device::Controller;
    const bool controller = profile.device != config::Device::Keyboard && pad.connected;
    auto active = [&](config::Action action) {
        const auto i = static_cast<std::size_t>(action);
        return (keyboard && profile.keys[i] > 0 && down(profile.keys[i]))
            || (controller && profile.pad[i] && (pad.buttons & profile.pad[i]) != 0);
    };
    double x=controller&&std::isfinite(pad.x)?pad.x:0;
    double y=controller&&std::isfinite(pad.y)?pad.y:0;
    unsigned buttons=0;
    if(active(config::Action::Up)||y < -profile.deadzone)buttons|=Thrust;
    if(active(config::Action::Down)||y > profile.deadzone)buttons|=Formation;
    if(active(config::Action::Left)||x < -profile.deadzone)buttons|=TurnLeft;
    if(active(config::Action::Right)||x > profile.deadzone)buttons|=TurnRight;
    if(active(config::Action::Fire))buttons|=Fire;
    if(active(config::Action::Stabilize))buttons|=Stabilize;
    if(active(config::Action::Deploy))buttons|=DeploySelected;
    if(active(config::Action::Construct2))buttons|=Construct2;
    return buttons;
}
class KeyboardState {
    unsigned held_ = 0, previous_ = 0;
    std::uint64_t tick_ = 0;
    bool sampled_ = false;
    int selected_construct_ = 1;
public:
    void Reset() noexcept { held_ = previous_ = 0; tick_ = 0; sampled_ = false; selected_construct_ = 1; }
    // Read the owning Powers.selectedConstruct before sampling to make U (or
    // an XInput semantic DeploySelected bit) deploy that native slot. The
    // numeric keys always invoke the game's direct pressConstruct1..5 paths.
    void SetSelectedConstruct(int slot) noexcept {
        if (slot >= 1 && slot <= 5) selected_construct_ = slot;
    }
    // Advance exactly once per simulation/input tick. Repeated event/helper
    // calls in that tick see identical edges. No native event increments this.
    void Update(std::uint64_t tick, unsigned held_buttons, bool expand_selected = true) noexcept {
        if (sampled_ && tick == tick_) return;
        previous_ = held_; held_ = held_buttons & AllButtons;
        if (expand_selected && (held_ & DeploySelected)) held_ |= Construct1 << (selected_construct_ - 1);
        tick_ = tick; sampled_ = true;
    }
    template<class IsDown>
    void SamplePhysical(std::uint64_t tick, IsDown&& down) noexcept {
        unsigned buttons = 0;
        for (const auto& binding : Bindings) if (down(binding.key)) buttons |= binding.button;
        Update(tick, buttons);
    }
    // Existing four-argument bot calls retain their movement/fire behavior.
    // Parent XInput slot1 or QA can supply native action bits as the fifth arg.
    void SampleBot(std::uint64_t tick, bool thrust, double turn, bool fire, unsigned actions = 0) noexcept {
        Update(tick, (thrust?Thrust:0u) | (turn < -0.01?TurnLeft:0u)
            | (turn > 0.01?TurnRight:0u) | (fire?Fire:0u) | actions);
    }
    template<class IsDown>
    void SampleConfigured(std::uint64_t tick, const config::Profile& profile, IsDown&& down,
                          double native_angle, const ControllerFrame& pad = {}, bool forward_only = false) noexcept {
        Update(tick,ConfiguredButtons(profile,down,native_angle,pad,forward_only));
    }
    template<class IsDown>
    void SampleConfiguredMenu(std::uint64_t tick, const config::Profile& profile, IsDown&& down,
                              const ControllerFrame& pad = {}) noexcept {
        // Reroll's U token must not expand into a construct token: selected
        // construct2 would otherwise also activate native menu Banish/8.
        Update(tick,ConfiguredMenuButtons(profile,down,pad),false);
    }
    bool Check(int key, Edge edge) const noexcept {
        const unsigned mask = KeyMask(key);
        switch (edge) {
            case Edge::Held: return (held_ & mask) != 0;
            case Edge::Pressed: return (held_ & ~previous_ & mask) != 0;
            case Edge::Released: return (~held_ & previous_ & mask) != 0;
        }
        return false;
    }
    // Mirrors native configured-key vararg OR semantics for P2. All unknown,
    // unassigned, mouse and P1 keys remain false. Caller writes the returned
    // boolean to script out using its RValue lifetime-safe assignment helper.
    bool EvaluateButtonHelper(int argc, native::RValue* const* args, Edge edge) const noexcept {
        if (argc <= 0 || !args) return false;
        for (int i = 0; i < argc; ++i) {
            double code = 0;
            if (Numeric(args[i], code) && code >= 8 && code <= 255
                && code == std::floor(code) && Check(static_cast<int>(code), edge)) return true;
        }
        return false;
    }
};

inline bool IsSoftwareCursorSelector(const native::RValue* selector) noexcept {
    if (!selector) return false;
    if ((selector->kind & 0xffffffu) == 15)
        return static_cast<std::uint64_t>(selector->integer) == (0x0100000000000000ull | SoftwareCursorAsset);
    double number = 0; return Numeric(selector, number) && number == SoftwareCursorAsset;
}
// Use only when active_owner==2. Covers direct release/value queries and
// deadzone setters as well as the obvious held/axis paths; no P1 poll can
// activate P2 or change a shared device deadzone during P2 native execution.
inline bool SuppressGamepadBuiltin(const char* name) noexcept {
    return name && std::strncmp(name, "gamepad_", 8) == 0;
}
// instance_exists hook: return false only for this exact asset selector in
// owner 2. An actual instance reference or other asset is never hidden here.
inline bool HideInstanceExists(int owner, int argc, native::RValue* const* args) noexcept {
    return owner == 2 && argc == 1 && args && IsSoftwareCursorSelector(args[0]);
}
}} // namespace ndship::input
