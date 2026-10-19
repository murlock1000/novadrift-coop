#include "../src/ship_input.hpp"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

namespace config = ndship::config;
namespace input = ndship::input;

static unsigned checks = 0;
static void Check(bool value, const char* description) {
    ++checks;
    if (!value) {
        std::fprintf(stderr, "FAIL: %s\n", description);
        std::exit(1);
    }
}

static unsigned Flight(const config::Profile& profile,
                       std::initializer_list<int> keys,
                       double angle, bool forward_only = true) {
    auto down = [&](int key) {
        for (int held : keys) if (key == held) return true;
        return false;
    };
    return input::ConfiguredButtons(profile, down, angle, {}, forward_only);
}

int main() {
    const config::Profile p2;
    Check(p2.steering == config::Steering::Tank,
          "new P2 profiles use thrust and rotation steering");
    Check(p2.Key(config::Action::Up) == 'I'
          && p2.Key(config::Action::Left) == 'J'
          && p2.Key(config::Action::Right) == 'L',
          "P2 flight defaults remain I thrust and J/L rotate");
    Check(p2.Key(config::Action::Down) == 'K',
          "K remains available for menu navigation");

    // The same physical button must mean the same rotation at every heading.
    // This catches accidentally retaining the heading-seeking directional mode.
    for (double angle : {0.0, 45.0, 90.0, 180.0, 270.0, -90.0, 360.0}) {
        Check(Flight(p2, {'I'}, angle) == input::Thrust,
              "I thrusts forward at every native heading");
        Check(Flight(p2, {'J'}, angle) == input::TurnLeft,
              "J always rotates left without thrust");
        Check(Flight(p2, {'L'}, angle) == input::TurnRight,
              "L always rotates right without thrust");
        Check(Flight(p2, {'K'}, angle) == 0,
              "menu-down K supplies no P2 flight input");
        Check(Flight(p2, {'I', 'K'}, angle) == input::Thrust,
              "holding menu-down cannot cancel forward thrust");
        Check(Flight(p2, {'I', 'J'}, angle)
                  == (input::Thrust | input::TurnLeft),
              "forward thrust and left rotation can be combined");
        Check(Flight(p2, {'I', 'L'}, angle)
                  == (input::Thrust | input::TurnRight),
              "forward thrust and right rotation can be combined");
    }

    Check(p2.Key(config::Action::Stabilize) == 0xa1
          && Flight(p2, {0xa1}, 90) == input::Stabilize,
          "Right Shift retains the separate stabilize special action");
    Check(Flight(p2, {'N'}, 90) == input::Fire,
          "changing steering preserves the fire binding");

    // The tray decides whether Inspect owns input. Once it does, its normal
    // native-menu translator must still turn O+K into down, rather than Back.
    const auto inspect_down = [](int key) { return key == 'O' || key == 'K'; };
    Check(input::ConfiguredMenuButtons(p2, inspect_down) == input::Formation,
          "Inspect plus K navigates down through the native menu token");
    Check(Flight(p2, {'K'}, 90, false) == input::Stabilize,
          "P1's unchanged tank translator keeps its Down brake");

    config::Profile directional = p2;
    directional.steering = config::Steering::Directional;
    Check(Flight(directional, {'I'}, 0) == input::TurnLeft,
          "explicit directional mode still turns toward Up");
    Check(Flight(directional, {'I'}, 90) == input::Thrust,
          "explicit directional mode thrusts when aligned with Up");
    Check(Flight(directional, {'J'}, 180) == input::Thrust,
          "explicit directional Left remains a heading rather than rotation");
    Check(Flight(directional, {'K'}, 270) == input::Thrust,
          "explicit directional mode retains its optional Down heading");

    std::printf("PASS: %u three-button control checks\n", checks);
}
