#pragma once
// Native build-state policy for Nova Drift 1.2.38.4, executable SHA256
// 6555f9f49c9b44f2a38dafc0846dd8af0e7e1f1e100cba976db60967b7173e2d.
// All operations run on the game thread. Cell RVAs contain runtime int32 IDs.
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include "native_abi.hpp"

namespace ndship::build {
namespace native = novadrift_native;
inline constexpr int ControllerAsset=0x179; // decimal377, not379
inline constexpr int PowersAsset=621, PlayerAsset=603, UpgradeUIAsset=828;
struct Field { const char* name; std::uintptr_t cell; };

// Owner0/1 retain the runner's real global slots. Only owner2 uses this overlay.
// levelup is the unspent budget; upgrades counts acquired upgrades.
inline constexpr std::array<Field, 14> IndependentGlobals{{
    {"levelup",0x1af8ce8}, {"upgrades",0x1af8938},
    {"upgradesPurchased",0x1b004c8}, {"upgradePurchased",0x1b00488},
    {"upgradeCancelled",0x1b00458}, {"banishedMod",0x1afa268},
    {"superModCount",0x1a9e3b8}, {"ultraChaosEligible",0x1b004d8},
    {"achievedUltraChaos",0x1a9e348}, {"canGainUpgrades",0x1a9e358},
    {"upgradePointsEarned",0x1aee548}, {"addedEffectiveLevel",0x1af8bc8},
    {"effectiveLevel",0x1af98c8}, {"rerollCount",0x1af6bb8}
}};
inline constexpr std::array<Field, 3> SharedTeamGlobals{{
    {"level",0x1af9148}, {"baseLevel",0x1afd8f8}, {"maxLevel",0x1a9e388}
}};
// These belong to the SINGLE shared Controller377, not global or Powers621.
inline constexpr std::array<Field, 2> ControllerXP{{
    {"xp",0x1a9da98}, {"maxexp",0x1afd8e8}
}};
inline constexpr std::array<Field, 10> OwnedLists{{
    {"gearShields",0x1af77b8}, {"gearBodies",0x1af7948},
    {"gearWeapons",0x1af79f8}, {"mods",0x1af75d8},
    {"wildMods",0x1afa228}, {"wildModsRare",0x1afa2d8},
    {"wildModsVeryRare",0x1afa2c8}, {"playerMods",0x1af7678},
    {"banishMods",0x1af7648}, {"previousMods",0x1a9de48}
}};

struct Rvas {
    static constexpr std::uintptr_t GainLevelUps=0x2fc6c0, UpdateScore=0x781240,
        ExpControl=0x2def70, CalculateMaxExp=0xbc220,
        GotoUpgrade=0x357de0, GotoUpgradeDeferred=0x358560,
        LeaveUpgrade=0x3a92e0, LeaveUpgradeDeferred=0x3a9a20,
        ConfirmUpgrade=0x1053f0, FinishUpgrade=0x2f2600,
        ControllerAlarm2=0x97d840,
        PowersCreate=0xc77c90, PowersCleanup=0xc955d0,
        RefreshLoadout=0x4c79f0, InitializeSatFireShooters=0x5fe60,
        RVFree=0x2bdb0, RVCopy=0x2bee0;
};
inline constexpr Field ControllerCanLevel{"canLevel",0x1a9de78};
inline constexpr Field ControllerPendingDeferred{"pendingDeferred",0x1aed4a8};
inline constexpr Field PowersRerolls{"rerolls",0x1afa278};
// Rerolls themselves belong to Powers; only the UI rerollCount is global.

inline bool Number(const native::RValue& value, double& result) noexcept {
    switch (value.kind & 0xffffffu) {
    case 0: case 13: result=value.real; break;
    case 7: result=static_cast<std::int32_t>(value.integer); break;
    case 10: result=static_cast<double>(value.integer); break;
    default: return false; // typed references are not progression numbers
    }
    return std::isfinite(result);
}

// Pass runner/SDK managed operations, never memcpy an owning managed RValue.
// copy constructs into an empty destination; free releases one existing value.
struct ManagedOps {
    void (*copy)(native::RValue*,const native::RValue*)=nullptr;
    void (*free)(native::RValue*)=nullptr;
    explicit operator bool() const noexcept { return copy && free; }
};
inline native::RValue Undefined() noexcept { native::RValue v{};v.kind=5;return v; }
inline bool IsManaged(const native::RValue& value) noexcept {
    return ((1u << (value.kind & 31u)) & 0x46u)!=0;
}

class GlobalOverlay {
    std::array<int,IndependentGlobals.size()> ids_{};
    std::array<native::RValue,IndependentGlobals.size()> values_{};
    ManagedOps ops_{};
    bool ready_=false;
public:
    GlobalOverlay() noexcept { for (auto& value:values_) value=Undefined(); }
    GlobalOverlay(const GlobalOverlay&)=delete;
    GlobalOverlay& operator=(const GlobalOverlay&)=delete;
    // No destructor calls into an engine that may already be unloaded. Call
    // Clear on the game thread while the runner is still alive, before re-init.
    void Clear() {
        ready_=false;
        for (auto& value:values_) {
            if (ops_ && IsManaged(value)) ops_.free(&value);
            value=Undefined();
        }
        ids_.fill(-1);
        ops_={};
    }
    // raw_get MUST bypass the overlay detour. Capture before P1 augments.
    bool Initialize(std::uintptr_t base,native::Instance* globals,
                    native::GetSlot raw_get,ManagedOps ops) {
        if (!base || !globals || !raw_get || !ops) return false;
        Clear(); ops_=ops;
        for (std::size_t i=0;i<IndependentGlobals.size();++i) {
            ids_[i]=*reinterpret_cast<const int*>(base+IndependentGlobals[i].cell);
            auto* source=raw_get(globals,ids_[i]);
            if (!source) { Clear();return false; }
            ops_.copy(&values_[i],source);
        }
        ready_=true;return true;
    }
    bool Ready() const noexcept { return ready_; }
    // Stable addresses are essential: YYC caches returned slot pointers.
    native::RValue* Slot(int owner,int field) noexcept {
        if (!ready_ || owner!=2) return nullptr;
        for (std::size_t i=0;i<ids_.size();++i)
            if (ids_[i]==field) return &values_[i];
        return nullptr;
    }
    native::RValue* Cell(std::uintptr_t cell) noexcept {
        if (!ready_) return nullptr;
        for (std::size_t i=0;i<IndependentGlobals.size();++i)
            if (IndependentGlobals[i].cell==cell) return &values_[i];
        return nullptr;
    }
    bool SetNumber(std::uintptr_t cell,double number) {
        auto* value=Cell(cell);
        if (!value || !std::isfinite(number)) return false;
        if (IsManaged(*value)) ops_.free(value);
        *value={};value->real=number;return true;
    }
    bool AddNumber(std::uintptr_t cell,double delta) {
        auto* value=Cell(cell);double before;
        return value && Number(*value,before) && SetNumber(cell,before+delta);
    }
    // Call AFTER one canonical original GainLevelUps invocation. Budget reads
    // must be copied numeric values captured before/after, not cached pointers.
    bool GrantEarnedChoices(double before,double after) {
        return std::isfinite(before) && std::isfinite(after) && after>=before &&
            AddNumber(0x1af8ce8,after-before);
    }
};

enum class CloneAction {
    CopyValue,          // managed assignment; includes immutable typed assets
    CloneVariable,      // variable_clone for mutable arrays/structs
    CopyOwnedList,      // ds_list_create + ds_list_copy; never share the handle
    ResetNoone,        // discard P1-owned runtime instance; rebuild in P2 context
    ResetZero,
    RebuildShooterArray // initializeSatFireShooters in P2 context
};
inline CloneAction PowersFieldPolicy(std::string_view name,const native::RValue& value) noexcept {
    for (const auto& list:OwnedLists)
        if (name==list.name) return CloneAction::CopyOwnedList;
    if (name=="shieldBase" || name=="shieldIndicator" || name=="shieldExtra" || name=="satFireID")
        return CloneAction::ResetNoone;
    if (name=="satFireShooterPool" || name=="satFireShooterShooting")
        return CloneAction::RebuildShooterArray;
    if (name=="satFireShooterCount") return CloneAction::ResetZero;
    const auto kind=value.kind & 0xffffffu;
    return kind==2 || kind==6 ? CloneAction::CloneVariable : CloneAction::CopyValue;
}
// kind6 may also be an instance/object reference. variable_clone preserves
// instances, so caller MUST separately remap references equal to P1/P1Powers
// and audit live-instance IDs inside mutable arrays. Enumerate custom fields
// with variable_instance_get_names; do not clone native instance allocations,
// id, room links, vtables or alarms. On failed partial initialization suppress
// normal Powers Cleanup until all ten owned list handles are safely initialized.

} // namespace ndship::build
