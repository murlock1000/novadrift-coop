#pragma once
// One native upgrade screen, served serially to P1 then P2. Pinned build only.
// Install/process/reset on the game thread. No second level award is invoked.
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "ship_context.hpp"
#include "ship_build.hpp"

namespace ndship::upgrade {
namespace native = novadrift_native;
struct Callbacks {
    // Numeric copied budgets. Return0 for owner2 before its ship/build is ready.
    double (*ReadBudget)(int owner)=nullptr;
    native::Instance* (*GetController)()=nullptr;
    // Unfiltered engine count for asset828; do not use owner-limited query hooks.
    int (*CountUI)()=nullptr;
    void (*Log)(const char*)=nullptr;
};
namespace detail {
inline Callbacks callbacks{};
inline std::uintptr_t base=0;
inline bool installed=false, pumping=false, request_second=false;
inline int session_owner=0;
inline bool close_completed=false, opening_failed=false;
inline std::uint64_t open_epoch=0;
inline ULONGLONG last_poll=0;
inline native::Script original_goto=nullptr, original_goto_deferred=nullptr,
    original_leave=nullptr, original_leave_deferred=nullptr;
inline native::Event original_alarm2=nullptr;
inline void Log(const char* text) { if(callbacks.Log) callbacks.Log(text); }
inline void LogOwner(const char* action,int owner) {
    char text[160];std::snprintf(text,sizeof text,"native upgrade %s owner=%d",action,owner);Log(text);
}
inline int EffectiveOwner() noexcept {
    return session_owner ? session_owner : (ndship::active_owner==2 ? 2 : 1);
}
inline bool Accepted(const native::RValue* result) noexcept {
    double number=0;return result && build::Number(*result,number) && number!=0;
}
inline native::RValue* False(native::RValue* result) noexcept {
    if(result) {
        if(build::IsManaged(*result))
            reinterpret_cast<void(*)(native::RValue*)>(base+build::Rvas::RVFree)(result);
        *result={};result->kind=13;
    }
    return result;
}
inline const native::RValue* Slot(native::Instance* controller,std::uintptr_t cell) {
    if(!controller) return nullptr;
    const auto table=*reinterpret_cast<void***>(controller);
    const int field=*reinterpret_cast<const int*>(base+cell);
    return reinterpret_cast<native::GetSlot>(table[1])(controller,field);
}
inline bool NumberAt(native::Instance* controller,std::uintptr_t cell,double& result) {
    const auto* value=Slot(controller,cell);return value && build::Number(*value,result);
}
inline bool NoPending(native::Instance* controller) {
    const auto* value=Slot(controller,build::ControllerPendingDeferred.cell);
    if(!value) return false;
    if((value->kind & 0xffffffu)==5) return true; // native initial/cleared value
    double number=0;return build::Number(*value,number) && number==0;
}
inline bool WorldReady(native::Instance* controller) {
    if(!controller || callbacks.CountUI()!=0 || !NoPending(controller)) return false;
    double can_level=0,pause=0,game_over=0;
    return NumberAt(controller,build::ControllerCanLevel.cell,can_level) && can_level!=0 &&
        NumberAt(controller,0x1b004b8,pause) && pause==0 &&
        NumberAt(controller,0x1af7718,game_over) && game_over==0;
}
inline native::RValue* GotoHook(native::Instance* self,native::Instance* other,
                               native::RValue* out,int argc,native::RValue** args) {
    const int requested=ndship::active_owner==2 ? 2 : (session_owner ? session_owner : 1);
    if(session_owner && session_owner!=requested) {
        if(requested==2) request_second=true;
        return False(out);
    }
    // Owner2 can only acquire the shared screen after all P1 choices are spent.
    // Same-owner native chaining remains unchanged once a session is active.
    if(requested==2 && !session_owner) {
        ndship::OwnerScope canonical(1);
        const double first=callbacks.ReadBudget(1),second=callbacks.ReadBudget(2);
        if(!std::isfinite(first)||!std::isfinite(second)||first>0||second<=0||!WorldReady(self)) {
            request_second=true;return False(out);
        }
    }
    const bool starting=session_owner==0;
    session_owner=requested;
    ndship::OwnerScope scope(requested);
    auto* result=original_goto(self,other,out,argc,args);
    if(Accepted(result)) {
        ++open_epoch;close_completed=false;opening_failed=false;
        if(requested==2) request_second=false;
        LogOwner("queued",requested);
    } else if(starting) {
        session_owner=0;close_completed=false;
    }
    return result;
}
inline native::RValue* GotoDeferredHook(native::Instance* self,native::Instance* other,
                                       native::RValue* out,int argc,native::RValue** args) {
    const int owner=EffectiveOwner();
    session_owner=owner;close_completed=false;
    ndship::OwnerScope scope(owner);LogOwner("opening",owner);
    auto* result=original_goto_deferred(self,other,out,argc,args);
    opening_failed=callbacks.CountUI()==0;
    if(opening_failed) LogOwner("opening declined by native eligibility",owner);
    return result;
}
inline native::RValue* LeaveHook(native::Instance* self,native::Instance* other,
                                native::RValue* out,int argc,native::RValue** args) {
    const int owner=EffectiveOwner();
    ndship::OwnerScope scope(owner);
    return original_leave(self,other,out,argc,args);
}
inline native::RValue* LeaveDeferredHook(native::Instance* self,native::Instance* other,
                                        native::RValue* out,int argc,native::RValue** args) {
    const int owner=EffectiveOwner();const auto epoch=open_epoch;
    ndship::OwnerScope scope(owner);
    auto* result=original_leave_deferred(self,other,out,argc,args);
    // A successful goto during original means the native same-owner chain won.
    if(session_owner && open_epoch==epoch) close_completed=true;
    LogOwner("leave deferred returned",owner);
    return result;
}
inline void Alarm2Hook(native::Instance* self,native::Instance* other) {
    ndship::OwnerScope scope(EffectiveOwner());original_alarm2(self,other);
}
} // namespace detail

inline int CurrentMenuOwner() noexcept { return detail::session_owner ? detail::session_owner : 1; }
inline bool SessionActive() noexcept { return detail::session_owner!=0; }
inline void RequestSecondMenu() noexcept { detail::request_second=true; }
// Mandatory on every new run generation, before reusing any owner IDs.
inline void Reset() noexcept {
    detail::session_owner=0;detail::close_completed=false;detail::opening_failed=false;
    detail::last_poll=0;
    detail::request_second=false;++detail::open_epoch;
}

inline void ProcessPending() {
    using namespace detail;
    if(!installed || pumping) return;
    if(session_owner && !close_completed && !opening_failed) return;
    const auto now=GetTickCount64();
    if(last_poll && now-last_poll<50) return;
    last_poll=now;
    struct PumpGuard { PumpGuard(){pumping=true;}~PumpGuard(){pumping=false;} } guard;
    ndship::OwnerScope canonical(1);
    auto* controller=callbacks.GetController();
    if(!controller) return;
    // Never reopen in the finishUpgrade -> Alarm2 gap. Full native leave must
    // have returned, with no UI or pending script and normal gameplay restored.
    if(session_owner) {
        if((!close_completed && !opening_failed) || !WorldReady(controller)) return;
        LogOwner("session complete",session_owner);
        session_owner=0;close_completed=false;opening_failed=false;
        return; // hand off on a later safe callback, not inside native cleanup
    }
    if(GetAsyncKeyState(VK_F7)&1) request_second=true;
    const double first=callbacks.ReadBudget(1),second=callbacks.ReadBudget(2);
    // Automatic handoff also serves newly earned P2 choices if P1 just finished.
    if(!std::isfinite(first)||!std::isfinite(second)||first>0||second<=0||!WorldReady(controller)) return;
    // Automatic delivery and F7 have the same safety gates; no budget is edited.
    request_second=true;
    native::RValue result=build::Undefined();
    ndship::OwnerScope second_owner(2);
    GotoHook(controller,controller,&result,0,nullptr);
    if(build::IsManaged(result))
        reinterpret_cast<void(*)(native::RValue*)>(base+build::Rvas::RVFree)(&result);
}

inline bool Install(Aurie::AurieModule* module,std::uintptr_t base,Callbacks callbacks) {
    if(detail::installed) return true;
    if(!module||!base||!callbacks.ReadBudget||!callbacks.GetController||!callbacks.CountUI) return false;
    detail::base=base;detail::callbacks=callbacks;
    struct Hook {const char* name;std::uintptr_t rva;std::array<unsigned char,16> signature;void* detour;void** original;};
    const Hook hooks[]={
        {"Ship.Upgrade.Goto",build::Rvas::GotoUpgrade,{0x48,0x8b,0xc4,0x48,0x89,0x58,0x20,0x48,0x89,0x50,0x10,0x48,0x89,0x48,0x08,0x55},reinterpret_cast<void*>(detail::GotoHook),reinterpret_cast<void**>(&detail::original_goto)},
        {"Ship.Upgrade.GotoDeferred",build::Rvas::GotoUpgradeDeferred,{0x48,0x8b,0xc4,0x48,0x89,0x58,0x20,0x4c,0x89,0x40,0x18,0x48,0x89,0x50,0x10,0x48},reinterpret_cast<void*>(detail::GotoDeferredHook),reinterpret_cast<void**>(&detail::original_goto_deferred)},
        {"Ship.Upgrade.Leave",build::Rvas::LeaveUpgrade,{0x48,0x8b,0xc4,0x48,0x89,0x58,0x20,0x4c,0x89,0x40,0x18,0x48,0x89,0x50,0x10,0x48},reinterpret_cast<void*>(detail::LeaveHook),reinterpret_cast<void**>(&detail::original_leave)},
        {"Ship.Upgrade.LeaveDeferred",build::Rvas::LeaveUpgradeDeferred,{0x48,0x8b,0xc4,0x48,0x89,0x58,0x20,0x4c,0x89,0x40,0x18,0x48,0x89,0x50,0x10,0x48},reinterpret_cast<void*>(detail::LeaveDeferredHook),reinterpret_cast<void**>(&detail::original_leave_deferred)},
        {"Ship.Upgrade.Alarm2",build::Rvas::ControllerAlarm2,{0x48,0x83,0xec,0x68,0x48,0x8d,0x05,0x9d,0x9b,0xd8,0x00,0x48,0x89,0x44,0x24,0x48},reinterpret_cast<void*>(detail::Alarm2Hook),reinterpret_cast<void**>(&detail::original_alarm2)}
    };
    for(const auto& hook:hooks) {
        if(std::memcmp(reinterpret_cast<const void*>(base+hook.rva),hook.signature.data(),hook.signature.size())) {
            char text[160];std::snprintf(text,sizeof text,"native upgrade signature mismatch %s RVA=%llx",hook.name,static_cast<unsigned long long>(hook.rva));detail::Log(text);return false;
        }
    }
    unsigned created=0;
    for(const auto& hook:hooks) {
        auto status=Aurie::MmCreateHook(module,hook.name,reinterpret_cast<void*>(base+hook.rva),hook.detour,hook.original);
        if(!Aurie::AurieSuccess(status)) {
            char text[160];std::snprintf(text,sizeof text,"native upgrade hook %s failed %s",hook.name,Aurie::AurieStatusToString(status));detail::Log(text);
            while(created) Aurie::MmRemoveHook(module,hooks[--created].name);
            return false;
        }
        ++created;
    }
    Reset();detail::installed=true;detail::Log("native serial upgrade hooks installed");return true;
}
} // namespace ndship::upgrade
