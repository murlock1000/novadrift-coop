#pragma once
// Lifecycle helpers for the pinned Nova Drift build. All callbacks and methods
// run on the engine thread. No object selectors or visible-instance queries are
// used for teardown: an exact owned ID can be inactive while a menu is open.
#include <algorithm>
#include <unordered_set>
#include <vector>
#include "ship_context.hpp"

namespace ndship::lifecycle {
namespace native=novadrift_native;
enum class Reason { Restart, CanonicalRespawn, JoinFailure, ShipDeath };
struct Report {
    unsigned retired=0, missing=0, retained=0, failed=0, already_retiring=0;
};
struct Callbacks {
    std::vector<int>(*owned_ids)(int owner)=nullptr;
    int(*owner_of)(native::Instance*)=nullptr;
    void(*forget)(int id,native::Instance*)=nullptr;
    void(*dispose_partial_list)(int handle)=nullptr;
    // begin must stop join/menu/stimuli; keep ownership and bindings until end.
    void(*begin)(Reason)=nullptr;
    // end may clear logical P2 IDs, bindings, UI state and overlays on resets.
    // ShipDeath retains Powers/build, so it must only mark that ship unavailable.
    void(*end)(Reason,const Report&)=nullptr;
    void(*log)(const char*)=nullptr;
};
namespace detail {
inline std::uintptr_t base=0;
inline Callbacks callbacks{};
inline thread_local unsigned teardown_depth=0;
inline std::unordered_set<int> blank_powers;
inline std::unordered_map<int,std::vector<int>> partial_lists;
inline native::Script original_restart=nullptr;
inline native::Event original_powers_cleanup=nullptr;
using Activate=void(*)(native::Instance*,native::Instance*,int,void*);
using Destroy=void(*)(native::Instance*,native::Instance*,int,native::RValue**);
inline Activate activate=nullptr;
inline Destroy destroy=nullptr;
inline bool installed=false;
inline void Log(const char* text) {if(callbacks.log)callbacks.log(text);}
class TeardownScope {
public: TeardownScope(){++teardown_depth;}~TeardownScope(){--teardown_depth;}
};
}
inline bool IsTearingDown() noexcept {return detail::teardown_depth!=0;}
inline void TrackBlankPowers(int id) {if(id>=100000)detail::blank_powers.insert(id);}
inline void TrackPowersList(int id,int handle) {if(id>=100000&&handle>=0&&detail::blank_powers.count(id))detail::partial_lists[id].push_back(handle);}
inline void MarkPowersInitialized(int id) {detail::blank_powers.erase(id);detail::partial_lists.erase(id);}

// Hash lookup deliberately accepts inactive flag2 and does not require active
// flag4. It still validates the native instance class and both identities.
inline native::Instance* ResolveExisting(int id) {
    using ndship::detail::Readable;
    if(id<100000 || !detail::base)return nullptr;
    const auto table=*reinterpret_cast<std::uintptr_t*>(detail::base+0x1a04228);
    const auto mask=*reinterpret_cast<std::uint32_t*>(detail::base+0x1a04230);
    if(!table||mask>0xffffff)return nullptr;
    const auto* bucket=reinterpret_cast<const std::uintptr_t*>(table+static_cast<std::uintptr_t>(mask&static_cast<unsigned>(id))*16);
    if(!Readable(bucket,8))return nullptr;
    auto node=*bucket;
    for(unsigned n=0;node&&n<4096;++n) {
        if(!Readable(reinterpret_cast<void*>(node),32))return nullptr;
        if(*reinterpret_cast<int*>(node+16)==id) {
            auto* instance=*reinterpret_cast<native::Instance**>(node+24);
            if(!Readable(instance,0xc8))return nullptr;
            const auto p=reinterpret_cast<std::uintptr_t>(instance);
            if(*reinterpret_cast<int*>(p+0x7c)!=1 || *reinterpret_cast<int*>(p+0xbc)!=id)return nullptr;
            return instance;
        }
        node=*reinterpret_cast<std::uintptr_t*>(node+8);
    }
    return nullptr;
}
inline int AssetOf(native::Instance* instance) {
    return instance&&ndship::detail::Readable(instance,0xc8)?*reinterpret_cast<int*>(reinterpret_cast<std::uintptr_t>(instance)+0xc0):-1;
}
inline unsigned FlagsOf(native::Instance* instance) {
    return instance&&ndship::detail::Readable(instance,0xbc)?*reinterpret_cast<unsigned*>(reinterpret_cast<std::uintptr_t>(instance)+0xb8):0x100001;
}
inline bool KeepBuildOnDeath(int asset) noexcept {
    // Gear and augment instances descend from oUpgrades829. Their indexed
    // definitions are1..229 in this exact build; Powers owns the cloned lists.
    return asset==621 || asset==829 || (asset>=1 && asset<=229);
}
inline bool ShouldRunNativePlayerDestroy(int owner,bool partner_alive) noexcept {
    // Native Destroy stops all sounds and touches shared black-hole objects.
    return !IsTearingDown() && !partner_alive && owner>=1 && owner<=2;
}

inline bool DestroyExact(int id,int owner) {
    auto* instance=ResolveExisting(id);
    if(!instance||!detail::activate||!detail::destroy || !detail::callbacks.owner_of || detail::callbacks.owner_of(instance)!=owner)return false;
    const auto flags=FlagsOf(instance);
    if(flags&0x100001)return true; // cleanup already ran / engine owns retirement.
    OwnerScope scope(owner);
    // Native instance_activate_object accepts a single ID and only reactivates
    // that target; no frame is advanced before destruction. Without this step,
    // native destroy skips inactive flag2 while an upgrade menu is paused.
    if(flags&2)detail::activate(instance,instance,id,nullptr);
    native::RValue reference{};reference.kind=15;reference.integer=static_cast<std::int64_t>(0x0400000100000000ull|static_cast<std::uint32_t>(id));
    native::RValue run_destroy{};run_destroy.real=0;
    native::RValue* args[]={&reference,&run_destroy};
    // execute_event=false bypasses global Destroy side effects. The runner
    // still executes native CleanUp event12, then marks the target retired.
    detail::destroy(instance,instance,2,args);
    const auto* remaining=ResolveExisting(id);
    return !remaining || (FlagsOf(const_cast<native::Instance*>(remaining))&0x100001)!=0;
}

inline Report TeardownOwner(int owner,Reason reason,int dying_ship_id=-1) {
    Report report;
    if(owner<1||owner>2 || !detail::callbacks.owned_ids || !detail::callbacks.owner_of) {report.failed=1;detail::Log("owner teardown missing required callbacks");return report;}
    detail::TeardownScope tearing_down;
    if(detail::callbacks.begin)detail::callbacks.begin(reason);
    auto ids=detail::callbacks.owned_ids(owner);
    std::sort(ids.begin(),ids.end());ids.erase(std::unique(ids.begin(),ids.end()),ids.end());
    // Equipment/construct cleanup runs with ship and Powers still available;
    // Player comes next and Powers DS-list destruction comes last.
    auto priority=[](int id){const int asset=AssetOf(ResolveExisting(id));return asset==621?2:asset==603?1:0;};
    std::stable_sort(ids.begin(),ids.end(),[&](int left,int right){return priority(left)<priority(right);});
    for(int id:ids) {
        auto* instance=ResolveExisting(id);
        if(!instance) {++report.missing;if(detail::callbacks.forget)detail::callbacks.forget(id,nullptr);continue;}
        if(detail::callbacks.owner_of(instance)!=owner) {++report.failed;continue;}
        if(id==dying_ship_id || (reason==Reason::ShipDeath&&KeepBuildOnDeath(AssetOf(instance)))) {++report.retained;continue;}
        if(FlagsOf(instance)&0x100001)++report.already_retiring;
        else if(DestroyExact(id,owner))++report.retired;
        else {++report.failed;continue;}
        if(detail::callbacks.forget)detail::callbacks.forget(id,instance);
    }
    if(detail::callbacks.end)detail::callbacks.end(reason,report);
    char text[240];std::snprintf(text,sizeof(text),"owner teardown owner=%d reason=%d retired=%u missing=%u retained=%u failed=%u retiring=%u",owner,static_cast<int>(reason),report.retired,report.missing,report.retained,report.failed,report.already_retiring);detail::Log(text);
    return report;
}

namespace detail {
inline native::RValue* RestartHook(native::Instance* self,native::Instance* other,native::RValue* out,int argc,native::RValue** argv) {
    if(IsTearingDown()) {Log("recursive restart suppressed during owned teardown");out->kind=5;return out;}
    // Canonical P1 is left to the original restart script. P2 is removed first,
    // even if that script's object-selector operations only target owner0/P1.
    TeardownOwner(2,Reason::Restart);
    TeardownScope restarting;
    OwnerScope canonical(0);
    return original_restart(self,other,out,argc,argv);
}
inline void PowersCleanupHook(native::Instance* self,native::Instance* other) {
    const int id=self&&ndship::detail::Readable(self,0xc8)?*reinterpret_cast<int*>(reinterpret_cast<std::uintptr_t>(self)+0xbc):-1;
    if(blank_powers.erase(id)) {
        const auto lists=partial_lists.find(id);
        if(lists!=partial_lists.end()) {
            if(callbacks.dispose_partial_list)for(int handle:lists->second)callbacks.dispose_partial_list(handle);
            else if(!lists->second.empty())Log("partial Powers lists require dispose_partial_list callback");
            partial_lists.erase(lists);
        }
        Log("partial blank Powers cleanup skipped: containers were not initialized");return;
    }
    original_powers_cleanup(self,other);
}
}

inline bool InstallLifecycleHooks(Aurie::AurieModule* module,std::uintptr_t base,Callbacks callbacks) {
    detail::callbacks=callbacks;detail::base=base;
    if(!callbacks.owned_ids||!callbacks.owner_of||!callbacks.forget) {detail::Log("lifecycle initialization requires ownership snapshot, resolver and forget callbacks");return false;}
    if(detail::installed)return true;
    const unsigned char restart_signature[16]={0x48,0x8b,0xc4,0x48,0x89,0x58,0x18,0x48,0x89,0x50,0x10,0x48,0x89,0x48,0x08,0x55};
    const unsigned char cleanup_signature[16]={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x55,0x57,0x41,0x56,0x48,0x8b};
    const unsigned char activate_signature[16]={0x40,0x53,0x48,0x83,0xec,0x20,0x49,0x63,0xd8,0x83,0xfb,0xff,0x75,0x23,0x48,0x85};
    const unsigned char destroy_signature[16]={0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57};
    if(std::memcmp(reinterpret_cast<void*>(base+0x4f1e90),restart_signature,16) || std::memcmp(reinterpret_cast<void*>(base+0xc955d0),cleanup_signature,16)
       || std::memcmp(reinterpret_cast<void*>(base+0xf5dc50),activate_signature,16) || std::memcmp(reinterpret_cast<void*>(base+0xf5de90),destroy_signature,16)) {detail::Log("lifecycle native signature mismatch");return false;}
    detail::activate=reinterpret_cast<detail::Activate>(base+0xf5dc50);
    detail::destroy=reinterpret_cast<detail::Destroy>(base+0xf5de90);
    auto status=Aurie::MmCreateHook(module,"NovaShip.Lifecycle.PowersCleanup",reinterpret_cast<void*>(base+0xc955d0),reinterpret_cast<void*>(detail::PowersCleanupHook),reinterpret_cast<void**>(&detail::original_powers_cleanup));
    if(!Aurie::AurieSuccess(status))return false;
    status=Aurie::MmCreateHook(module,"NovaShip.Lifecycle.Restart",reinterpret_cast<void*>(base+0x4f1e90),reinterpret_cast<void*>(detail::RestartHook),reinterpret_cast<void**>(&detail::original_restart));
    if(!Aurie::AurieSuccess(status)){Aurie::MmRemoveHook(module,"NovaShip.Lifecycle.PowersCleanup");return false;}
    detail::installed=true;detail::Log("native restart and Powers cleanup lifecycle hooks installed");return true;
}
} // namespace ndship::lifecycle
