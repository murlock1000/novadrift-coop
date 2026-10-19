#pragma once
// Per-player co-op controls; the original game preferences stay separate.
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace ndship { namespace config {
enum class Steering { Directional = 0, Tank = 1 };
enum class Device { Keyboard = 0, Controller = 1, Both = 2 };
enum class Action : std::size_t {
    Up, Left, Down, Right, Fire, Blink, Stabilize, Formation, Deploy,
    Construct1, Construct2, Construct3, Construct4, Construct5,
    StrafeLeft, StrafeRight, StrafeForward, StrafeBack, Strafe, Upgrade, Count
};
inline constexpr std::size_t ActionCount = static_cast<std::size_t>(Action::Count);
inline constexpr const char* ActionNames[ActionCount] = {
    "up","left","down","right","fire","blink","stabilize","formation","deploy",
    "construct1","construct2","construct3","construct4","construct5",
    "strafe_left","strafe_right","strafe_forward","strafe_back","strafe_modifier","upgrade"
};
inline constexpr const wchar_t* ActionLabels[ActionCount] = {
    L"Up / tank thrust",L"Left / turn left",L"Down / tank brake",L"Right / turn right",
    L"Fire",L"Blink",L"Stabilize / brake",L"Formation",L"Deploy selected",
    L"Construct 1",L"Construct 2",L"Construct 3",L"Construct 4",L"Construct 5",
    L"Strafe left",L"Strafe right",L"Strafe forward",L"Strafe back",L"Strafe modifier",L"Open / close upgrades"
};
// XInput button bits; triggers use two otherwise unused bits. Parent normalizes
// XInput data into these values, never into GameMaker's player1 device index.
inline constexpr std::uint32_t LeftTrigger = 0x10000, RightTrigger = 0x20000;
struct PadChoice { const wchar_t* label; std::uint32_t mask; };
inline constexpr PadChoice PadChoices[] = {
    {L"Unbound",0},{L"A",0x1000},{L"B",0x2000},{L"X",0x4000},{L"Y",0x8000},
    {L"LB",0x100},{L"RB",0x200},{L"LT",LeftTrigger},{L"RT",RightTrigger},
    {L"D-pad up",1},{L"D-pad down",2},{L"D-pad left",4},{L"D-pad right",8},
    {L"Left stick click",0x40},{L"Right stick click",0x80},{L"Back",0x20}
};
struct Profile {
    Steering steering = Steering::Directional;
    Device device = Device::Keyboard;
    int controller_slot = 1; // XInput zero-based slot; UI labels controllers1..4.
    double deadzone = 0.22;
    bool mouse_aim = false;
    std::array<int, ActionCount> keys = {{
        'I','J','K','L','N',0xa3,0xa1,'O','U',
        '7','8','9','0',0xbd,0xba,0xde,0xdb,0xdd,0xa5,0x76
    }};
    std::array<std::uint32_t, ActionCount> pad = {{
        1,4,2,8,RightTrigger,0x1000,0x2000,0x8000,0x4000,
        0,0,0,0,0,0,0,0,0,0x100,0x20
    }};
    int Key(Action action) const noexcept { return keys[static_cast<std::size_t>(action)]; }
    std::uint32_t Pad(Action action) const noexcept { return pad[static_cast<std::size_t>(action)]; }
};
inline Profile PlayerOneDefaults() {
    Profile p;p.steering=Steering::Tank;p.mouse_aim=true;p.controller_slot=0;
    p.keys={{1,'A','S','D',2,32,0xa0,'F','E','1','2','3','4','5','Q','R','W','S',0xa4,4}};
    return p;
}
inline constexpr const wchar_t* PlayerOneFileName=L"p1-controls.ini";
inline constexpr const wchar_t* FileName = L"p2-controls.ini";
inline std::string KeyLabel(int key) {
    if(key<0)return "Unbound";
    if(key>=0x70&&key<=0x87)return "F"+std::to_string(key-0x70+1);
    if((key>='A'&&key<='Z')||(key>='0'&&key<='9'))return std::string(1,static_cast<char>(key));
    switch(key) {
        case 1:return "Mouse left";case 2:return "Mouse right";case 4:return "Mouse middle";
        case 5:return "Mouse back";case 6:return "Mouse forward";case 32:return "Space";
        case 0xa0:return "Left Shift";case 0xa1:return "Right Shift";
        case 0xa2:return "Left Ctrl";case 0xa3:return "Right Ctrl";
        case 0xa4:return "Left Alt";case 0xa5:return "Right Alt";
        case 0xba:return ";";case 0xde:return "'";case 0xdb:return "[";case 0xdd:return "]";case 0xbd:return "-";
        case 37:return "Left";case 38:return "Up";case 39:return "Right";case 40:return "Down";
        default:return "Key "+std::to_string(key);
    }
}
inline std::string PadLabel(std::uint32_t mask) {
    for(const auto& choice:PadChoices)if(choice.mask==mask){
        std::string label;
        for(const wchar_t* text=choice.label;*text;++text)label.push_back(*text<=0x7f?static_cast<char>(*text):'?');
        return label;
    }
    return "Unbound";
}
inline std::string Help(const Profile& profile, bool menu = false) {
    const bool pad=profile.device==Device::Controller;
    auto label=[&](Action action){return pad?PadLabel(profile.Pad(action)):KeyLabel(profile.Key(action));};
    const auto movement=pad?"left stick":label(Action::Up)+"/"+label(Action::Left)+"/"+label(Action::Down)+"/"+label(Action::Right);
    const auto prefix=pad?"P2 controller "+std::to_string(profile.controller_slot+1)+": ":"P2: ";
    if(menu)return prefix+movement+" navigate | "+label(Action::Fire)+" select | "+label(Action::Stabilize)+" back | "+label(Action::Deploy)+" reroll | "+label(Action::Construct2)+" banish";
    return prefix+movement+(profile.steering==Steering::Tank?" thrust/turn/brake":" move")+" | "+label(Action::Fire)+" fire | "+label(Action::Blink)+" blink | "+label(Action::Deploy)+" deploy | F6 controls";
}
inline bool Validate(const Profile& profile, std::string& error) {
    if (profile.steering != Steering::Directional && profile.steering != Steering::Tank) error = "Unknown steering mode.";
    else if (profile.device != Device::Keyboard && profile.device != Device::Controller && profile.device != Device::Both) error = "Unknown input device.";
    else if (profile.controller_slot < 0 || profile.controller_slot > 3) error = "Controller must be between 1 and 4.";
    else if (!std::isfinite(profile.deadzone) || profile.deadzone < 0.05 || profile.deadzone > 0.8) error = "Deadzone must be between 0.05 and 0.80.";
    else {
        for (std::size_t i = 0; i < ActionCount; ++i) {
            if (profile.keys[i] != -1 && (profile.keys[i] < 1 || profile.keys[i] > 255)) { error = std::string("Invalid key for ") + ActionNames[i]; return false; }
            bool valid = profile.pad[i] == 0;
            for (const auto& choice : PadChoices) valid = valid || choice.mask == profile.pad[i];
            if (!valid) { error = std::string("Unknown controller binding for ") + ActionNames[i]; return false; }
        }
        error.clear(); return true;
    }
    return false;
}
inline std::string Trim(std::string text) {
    auto a = text.find_first_not_of(" \r\n\t");
    return a == std::string::npos ? std::string() : text.substr(a, text.find_last_not_of(" \r\n\t") - a + 1);
}
// Parse transactionally. Missing entries retain defaults; malformed known
// entries reject the whole update so a running game keeps its last good file.
inline bool Parse(std::istream& stream, Profile& out, std::string& error) {
    Profile next;
    std::string section, line; unsigned line_number = 0;
    try {
        while (std::getline(stream, line)) {
            ++line_number; line = Trim(line);
            if (line.empty() || line[0] == ';' || line[0] == '#') continue;
            if (line.front() == '[' && line.back() == ']') { section = line.substr(1, line.size()-2); continue; }
            const auto split = line.find('=');
            if (split == std::string::npos) { error = "Missing '=' on line " + std::to_string(line_number); return false; }
            const auto key = Trim(line.substr(0,split)), value = Trim(line.substr(split+1));
            std::size_t used = 0;
            if (section == "controls") {
                if (key == "steering") { if (value == "directional") next.steering = Steering::Directional; else if (value == "tank") next.steering = Steering::Tank; else throw 0; }
                else if (key == "device") { if (value == "keyboard") next.device = Device::Keyboard; else if (value == "controller") next.device = Device::Controller; else if (value == "both") next.device = Device::Both; else throw 0; }
                else if (key == "controller_slot") { next.controller_slot = std::stoi(value,&used); if (used != value.size()) throw 0; }
                else if (key == "mouse_aim") { if(value=="1")next.mouse_aim=true;else if(value=="0")next.mouse_aim=false;else throw 0; }
                else if (key == "deadzone") { next.deadzone = std::stod(value,&used); if (used != value.size()) throw 0; }
            } else if (section == "keys" || section == "controller") {
                for (std::size_t i = 0; i < ActionCount; ++i) if (key == ActionNames[i]) {
                    const auto number = std::stoll(value,&used,0); if (used != value.size()) throw 0;
                    if (section == "keys") { if (number < -1 || number > 255) throw 0; next.keys[i] = static_cast<int>(number); }
                    else { if (number < 0 || number > 0x20000) throw 0; next.pad[i] = static_cast<std::uint32_t>(number); }
                }
            }
        }
    } catch (...) { error = "Invalid setting on line " + std::to_string(line_number); return false; }
    if (!Validate(next,error)) return false;
    out = next; return true;
}
inline bool Load(const std::filesystem::path& path, Profile& out, std::string& error) {
    std::ifstream stream(path);
    if (!stream) { error = "Cannot open P2 control profile."; return false; }
    return Parse(stream,out,error);
}
inline void Write(std::ostream& stream, const Profile& profile) {
    stream << "; Nova Drift co-op controls. Original game preferences are unchanged.\n"
           << "; controller_slot is zero-based; key codes are Windows virtual keys.\n[controls]\nsteering="
           << (profile.steering == Steering::Tank ? "tank" : "directional") << "\ndevice="
           << (profile.device == Device::Controller ? "controller" : profile.device == Device::Both ? "both" : "keyboard")
           << "\nmouse_aim=" << (profile.mouse_aim?1:0) << "\ncontroller_slot=" << profile.controller_slot << "\ndeadzone=" << profile.deadzone << "\n\n[keys]\n";
    for (std::size_t i = 0; i < ActionCount; ++i) stream << ActionNames[i] << '=' << profile.keys[i] << '\n';
    stream << "\n[controller]\n";
    for (std::size_t i = 0; i < ActionCount; ++i) stream << ActionNames[i] << '=' << profile.pad[i] << '\n';
}
inline bool Save(const std::filesystem::path& path, const Profile& profile, std::string& error) {
    if (!Validate(profile,error)) return false;
    auto temporary = path; temporary += L".tmp";
    { std::ofstream stream(temporary, std::ios::trunc); if (!stream) { error = "Cannot write P2 control profile."; return false; }
      Write(stream,profile); stream.flush(); if (!stream) { error = "Writing P2 control profile failed."; return false; } }
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) { error = "Cannot replace P2 control profile (Windows error " + std::to_string(GetLastError()) + ")."; return false; }
#else
    std::error_code ec; std::filesystem::rename(temporary,path,ec);
    if (ec) { error = "Cannot replace P2 control profile: " + ec.message(); return false; }
#endif
    error.clear(); return true;
}
// Parent may poll this on its existing command/file timer. Missing/invalid
// files preserve the current profile and are not written by the game.
class LiveProfile {
    Profile value_{};
    std::filesystem::file_time_type stamp_{};
    bool have_stamp_ = false;
public:
    const Profile& Get() const noexcept { return value_; }
    bool ReloadIfChanged(const std::filesystem::path& path, std::string& error) {
        error.clear();
        std::error_code ec; const auto stamp = std::filesystem::last_write_time(path,ec);
        if (ec) return false;
        if (have_stamp_ && stamp == stamp_) return false;
        stamp_ = stamp; have_stamp_ = true;
        Profile next; if (!Load(path,next,error)) return false;
        value_ = next; return true;
    }
};
}} // namespace ndship::config
