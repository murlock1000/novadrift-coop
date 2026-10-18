#pragma once
// P2 bindings for the native upgrade/loadout menus in the pinned game build.
// Only configured key IDs are overlaid; key state stays in KeyboardState.
#include <cstring>
#include "ship_input.hpp"

namespace ndship::menu_input {
namespace native=novadrift_native;
inline constexpr int Left='J',Right='L',Up='I',Down='O',Confirm='N',Back='K',Reroll='U',Banish='8';
inline constexpr input::FieldDefault Defaults[]={
    {"menuLeftKey",0x1afc5d8,Left}, {"menuRightKey",0x1afc7b8,Right},
    {"menuUpKey",0x1afc588,Up}, {"menuDownKey",0x1afc5e8,Down},
    {"confirmKey",0x1afc7d8,Confirm}, {"confirmKey2",0x1afc798,-1},
    {"backKey",0x1afc398,Back}, {"rerollKey",0x1afc618,Reroll},
    {"banishKey",0x1afc7a8,Banish},
    // The Controller also enters owner2 while P2 is the surviving ship. Keep
    // shared Escape available then: Input() delegates it to the native helper
    // outside upgrade sessions. P2's upgrade Back remains its own binding.
    // Native F7 upgrade requests still use the coordinator.
    {"menuKey",0x1afc698,27}, {"upgradeKey",0x1afc658,-1},
    {"player1",0x1afc718,-1}, {"gamepadInUse",0x1b01038,0},
    {"gamepadMenuLeft",0x1afc468,-1}, {"gamepadMenuRight",0x1afc448,-1},
    {"gamepadMenuUp",0x1afc498,-1}, {"gamepadMenuDown",0x1afc388,-1},
    {"gamepadConfirm",0x1afc538,-1}, {"gamepadMenuBack",0x1afc3c8,-1},
    {"gamepadReroll",0x1afc518,-1}, {"gamepadBanish",0x1afc4c8,-1},
    {"gamepadTogglePause",0x1afc428,-1}, {"gamepadUpgrade",0x1afc3b8,-1},
    {"mouseActive",0x1af1e88,0}
};
template<class SetOverlay>
inline void ConfigureMenuInputOverlay(SetOverlay&& set) {
    for(const auto& field:Defaults) set(field.cell_rva,input::Number(field.value));
}
inline bool IsMenuKey(int key) noexcept {
    return key==Left||key==Right||key==Up||key==Down||key==Confirm||key==Back||key==Reroll||key==Banish;
}
// Every default above is already in ship_input::Bindings, so no new input bits
// or raw P1 keyboard fallback is needed. Sample once per Controller Step tick,
// including while Player Step is deactivated by the upgrade screen.
inline bool EvaluateInput(const input::KeyboardState& state,int argc,
                          native::RValue* const* args,input::Edge edge) noexcept {
    return state.EvaluateButtonHelper(argc,args,edge);
}
// Observed direct native UI bypass: Upgrade Step calls this builtin directly,
// then device_mouse_x/y_to_gui and position_meeting. Returning false prevents
// P1's mouse click selecting P2's upgrade. Other keyboard presses go through
// inputButtonPressed and therefore through the existing owner2 script detour.
inline bool SuppressPointerButtonBuiltin(const char* name) noexcept {
    return name && (std::strcmp(name,"mouse_check_button_pressed")==0 ||
        std::strcmp(name,"mouse_check_button")==0 ||
        std::strcmp(name,"mouse_check_button_released")==0);
}
// Loadout Step also polls coordinates for hover. For owner2 menu scope return
// an off-screen coordinate for these two names, unless a P2 pointer exists.
inline bool IsPointerCoordinateBuiltin(const char* name) noexcept {
    return name && (std::strcmp(name,"device_mouse_x_to_gui")==0 ||
        std::strcmp(name,"device_mouse_y_to_gui")==0);
}
inline constexpr double OffscreenPointer=-1000000.0;
inline constexpr std::uintptr_t ControllerStep=0x97e5f0,UpgradeStep=0xe9b030,
    DeclineUpgrade=0x16ec70,MenuControls=0x3ed8d0;
// Controller Step must enter CurrentMenuOwner BEFORE original when a menu
// session is active: it owns Back/K, invoking declineUpgrade on UI828. Only
// setting UI828's owner leaves Controller's cached backKey pointed at P1.
} // namespace ndship::menu_input
