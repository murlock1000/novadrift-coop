#pragma once
// Owner-specific singleton selectors for the pinned Nova Drift 1.2.38.4 runner.
// Bindings and hooks do not create or destroy engine instances. The caller owns
// their lifetime and clears bindings before a room/run transition.
#include <windows.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <unordered_map>
#include <YYToolkit/YYTK_Shared.hpp>
#include "native_abi.hpp"
#include "ship_perf.hpp"

namespace ndship {
namespace native = novadrift_native;
inline thread_local int active_owner = 0;
class OwnerScope {
    int previous_;
public:
    explicit OwnerScope(int owner) noexcept : previous_(active_owner) { active_owner = owner; }
    ~OwnerScope() { active_owner = previous_; }
    OwnerScope(const OwnerScope&) = delete;
    OwnerScope& operator=(const OwnerScope&) = delete;
};

struct Stats {
    std::uint64_t read_redirects = 0;
    std::uint64_t write_redirects = 0;
    std::uint64_t with_redirects = 0;
    std::uint64_t with_filtered = 0;
    std::uint64_t query_filtered = 0;
};
namespace detail {
inline std::atomic<bool> enabled{true};
// 1024 covers all objects in this pinned build; an invalid index is rejected.
inline std::array<std::array<std::atomic<int>, 1024>, 3> bindings{};
inline std::atomic<std::uint64_t> read_redirects{0}, write_redirects{0}, with_redirects{0};
inline std::atomic<std::uint64_t> with_filtered{0}, query_filtered{0};
inline std::array<std::atomic<bool>,1024> owned_assets{};
// These registries are engine-thread-only, like the runner instance registry.
struct OwnerEntry { int owner; int instance_id; };
inline std::unordered_map<native::Instance*,OwnerEntry> owned_instances;
inline int (*owner_resolver)(native::Instance*) = nullptr;
inline std::uintptr_t runner_base = 0;
struct IteratorFilter { int owner; };
// Most native with statements nest only a few levels. Avoid allocating and
// freeing an unordered_map node on every group iteration; preserve an overflow
// map rather than imposing a correctness limit on unusual scripts.
class IteratorFilters {
    struct Entry { void* iterator; int owner; };
    std::array<Entry,32> inline_{};
    unsigned count_=0;
    std::unordered_map<void*,IteratorFilter> overflow_;
public:
    void erase(void* iterator) {
        for(unsigned i=0;i<count_;++i)if(inline_[i].iterator==iterator){inline_[i]=inline_[--count_];return;}
        if(!overflow_.empty())overflow_.erase(iterator);
    }
    void Set(void* iterator,int owner) {
        for(unsigned i=0;i<count_;++i)if(inline_[i].iterator==iterator){inline_[i].owner=owner;return;}
        if(!overflow_.empty()) {const auto found=overflow_.find(iterator);if(found!=overflow_.end()){found->second.owner=owner;return;}}
        if(count_<inline_.size()){inline_[count_++]={iterator,owner};return;}
        overflow_[iterator]={owner};
    }
    int Owner(void* iterator) const {
        for(unsigned i=0;i<count_;++i)if(inline_[i].iterator==iterator)return inline_[i].owner;
        if(overflow_.empty())return -1;
        const auto found=overflow_.find(iterator);
        return found==overflow_.end()?-1:found->second.owner;
    }
    bool empty() const noexcept {return !count_&&overflow_.empty();}
};
inline thread_local IteratorFilters iterator_filters;
using Read = bool(*)(const native::RValue*, int, std::int32_t, native::RValue*, bool, bool);
using ContextRead = bool(*)(const native::RValue*, int, std::int32_t, native::RValue*, native::Instance*, native::Instance*);
using Write = bool(*)(const native::RValue*, int, std::int32_t, native::RValue*);
using With = int(*)(void*, native::Instance**, native::Instance**, const native::RValue*);
using WithNext = bool(*)(void*, native::Instance**, native::Instance**);
using WithEnd = void(*)(void*, native::Instance**, native::Instance**);
using QueryBegin = void*(*)(void*,int,native::Instance*,native::Instance*,bool);
using QueryPeek = native::Instance*(*)(void*);
using QueryAdvance = void(*)(void*);
inline Read original_read = nullptr;
inline ContextRead original_context_read = nullptr;
inline Write original_write = nullptr;
inline Write original_builtin_write = nullptr;
inline With original_with = nullptr;
inline WithNext original_next = nullptr;
inline WithEnd original_end = nullptr;
inline QueryBegin query_begin = nullptr;
inline QueryPeek query_peek = nullptr;
inline QueryAdvance query_advance = nullptr;
inline YYTK::TRoutine original_exists = nullptr, original_find = nullptr, original_number = nullptr;
inline bool queries_installed = false;
inline bool installed = false;
inline void (*logger)(const char*) = nullptr;
inline void Log(const char* value) { if (logger) logger(value); }
}

inline bool Bind(int owner, int asset_index, int instance_id) noexcept {
    if (owner < 0 || owner > 2 || asset_index < 0 || asset_index >= 1024 || instance_id < 100000) return false;
    detail::bindings[owner][asset_index].store(instance_id, std::memory_order_relaxed);
    return true;
}
inline bool BindEmpty(int owner, int asset_index) noexcept {
    if (owner < 0 || owner > 2 || asset_index < 0 || asset_index >= 1024) return false;
    detail::bindings[owner][asset_index].store(-1, std::memory_order_relaxed);
    return true;
}
inline bool RegisterOwnedAsset(int asset_index, bool enabled = true) noexcept {
    if (asset_index < 0 || asset_index >= 1024) return false;
    detail::owned_assets[asset_index].store(enabled,std::memory_order_relaxed);
    return true;
}
inline bool RegisterOwnedInstance(native::Instance* instance, int owner, int instance_id=0) {
    if (!instance || owner < 1 || owner > 2) return false;
    if(instance_id && instance_id<100000)return false;
    detail::owned_instances[instance] = {owner,instance_id};
    return true;
}
// The caller supplies an ID captured from a currently live native callback or
// validated ID lookup. No pointer is dereferenced and no VirtualQuery is needed.
inline int RegisteredOwner(native::Instance* instance,int instance_id) {
    const auto found=detail::owned_instances.find(instance);
    if(found==detail::owned_instances.end())return 0;
    if(found->second.instance_id && found->second.instance_id!=instance_id)return 0;
    return found->second.owner;
}
// Raw lookup is memory-safe but cannot itself prove that an address was not
// recycled. Prefer the ID overload when identity has already been captured.
inline int RegisteredOwner(native::Instance* instance) {
    const auto found=detail::owned_instances.find(instance);
    return found==detail::owned_instances.end()?0:found->second.owner;
}
inline void UnregisterOwnedInstance(native::Instance* instance) { detail::owned_instances.erase(instance); }
inline void SetOwnerResolver(int(*resolver)(native::Instance*)) noexcept { detail::owner_resolver = resolver; }
inline void ClearOwnedInstances() { detail::owned_instances.clear(); }
inline void ClearOwnedAssets() noexcept { for(auto& asset:detail::owned_assets) asset.store(false,std::memory_order_relaxed); }
inline void ClearBindings() noexcept {
    for (auto& owner : detail::bindings) for (auto& binding : owner) binding.store(0, std::memory_order_relaxed);
}
inline void SetEnabled(bool value) noexcept { detail::enabled.store(value, std::memory_order_relaxed); }
inline Stats GetStats() noexcept {
    return {detail::read_redirects.load(std::memory_order_relaxed),
            detail::write_redirects.load(std::memory_order_relaxed),
            detail::with_redirects.load(std::memory_order_relaxed),
            detail::with_filtered.load(std::memory_order_relaxed),
            detail::query_filtered.load(std::memory_order_relaxed)};
}
inline void ResetStats() noexcept {
    detail::read_redirects.store(0, std::memory_order_relaxed);
    detail::write_redirects.store(0, std::memory_order_relaxed);
    detail::with_redirects.store(0, std::memory_order_relaxed);
    detail::with_filtered.store(0, std::memory_order_relaxed);
    detail::query_filtered.store(0, std::memory_order_relaxed);
}

// Only an object ASSET reference is remapped. Numeric object IDs, actual
// instance references and raw instance pointers retain their original meaning.
// The input can be a shared constant; never modify it or free a borrowed value.
inline const native::RValue* RemapAssetSelector(const native::RValue* selector, native::RValue& temporary) noexcept {
    const int owner = active_owner;
    if (!selector || owner < 0 || owner > 2 || !detail::enabled.load(std::memory_order_relaxed)
        || (selector->kind & 0xffffffu) != 15) return selector;
    const auto payload = static_cast<std::uint64_t>(selector->integer);
    if ((payload >> 32) != 0x01000000u) return selector;
    const auto asset = static_cast<std::uint32_t>(payload);
    if (asset >= 1024) return selector;
    const int id = detail::bindings[owner][asset].load(std::memory_order_relaxed);
    if (id == -1) {
        temporary = *selector;
        temporary.kind = 7;
        temporary.integer = -4; // noone, distinct from an unbound selector.
        return &temporary;
    }
    if (id < 100000) return selector;
    temporary = *selector;
    temporary.integer = static_cast<std::int64_t>(0x0400000100000000ull | static_cast<std::uint32_t>(id));
    return &temporary;
}

namespace detail {
inline int AssetIndex(const native::RValue* selector) noexcept {
    if (!selector) return -1;
    const auto kind=selector->kind&0xffffffu;
    if (kind==15 && (static_cast<std::uint64_t>(selector->integer)>>32)==0x01000000u) {
        const auto index=static_cast<std::uint32_t>(selector->integer);
        return index<1024?static_cast<int>(index):-1;
    }
    // Ordinary numeric object selectors also occur in builtins and with.
    if (kind==7) {const auto index=static_cast<std::int32_t>(selector->integer);return index>=0&&index<1024?index:-1;}
    if (kind==0 && selector->real>=0 && selector->real<1024 && std::floor(selector->real)==selector->real) return static_cast<int>(selector->real);
    return -1;
}
inline bool NeedsFilter(const native::RValue* selector) noexcept {
    if (!enabled.load(std::memory_order_relaxed) || active_owner<0 || active_owner>2) return false;
    const int asset=AssetIndex(selector);
    return asset>=0 && owned_assets[asset].load(std::memory_order_relaxed);
}
inline bool MatchesOwner(native::Instance* instance, int requested) {
    if (!instance) return false;
    int owner=0;
    const auto found=owned_instances.find(instance);
    if (found!=owned_instances.end()) {
        // Every caller passes a runner-live instance from native with or from a
        // validated native find result. Optional IDs reject address reuse with
        // no cache of VirtualQuery results or assumed object lifetime.
        if(!found->second.instance_id || *reinterpret_cast<const int*>(reinterpret_cast<std::uintptr_t>(instance)+0xbc)==found->second.instance_id)owner=found->second.owner;
        else owned_instances.erase(found);
    }
    if (!owner && owner_resolver) owner=owner_resolver(instance);
    // Existing untagged instances belong to canonical P1 for registered groups.
    if (owner!=1 && owner!=2) owner=1;
    return owner==(requested==0?1:requested);
}
template<class Visitor> inline void VisitOwned(const native::RValue* selector, Visitor visitor,std::uint64_t* skipped=nullptr) {
    perf::Span timing(perf::Metric::ContextQuery);
    std::uintptr_t iterator[4]{};
    native::Instance* self=nullptr;
    native::Instance* other=nullptr;
    const int count=original_with(iterator,&self,&other,selector);
    if(count>0) do {
        if(MatchesOwner(self,active_owner)) {if(!visitor(self)) break;}
        else if(skipped)++*skipped;
    } while(original_next(iterator,&self,&other));
    original_end(iterator,&self,&other);
}
inline int NativeFilteredFind(const native::RValue* selector,int wanted,native::Instance* self,native::Instance* other,int* total=nullptr) {
    perf::Span timing(perf::Metric::ContextQuery);
    // This is the same64-byte stack iterator and order used by this build's
    // native instance_find(f56ae0), whose nth-index call otherwise rescans it.
    alignas(16) std::array<std::uintptr_t,8> iterator{};
    query_begin(iterator.data(),AssetIndex(selector),self,other,false);
    int accepted=0,selected=-4;
    std::uint64_t skipped=0;
    for(auto* instance=query_peek(iterator.data());instance;instance=query_peek(iterator.data())) {
        query_advance(iterator.data());
        const auto address=reinterpret_cast<std::uintptr_t>(instance);
        if(*reinterpret_cast<const std::uint32_t*>(address+0xb8)&0x100003)continue;
        if(MatchesOwner(instance,active_owner)) {
            if(accepted==wanted)selected=*reinterpret_cast<const int*>(address+0xbc);
            ++accepted;
            if(!total&&selected>=100000)break;
        } else ++skipped;
    }
    if(skipped)query_filtered.fetch_add(skipped,std::memory_order_relaxed);
    if(total)*total=accepted;
    // Like native instance_find, this stack iterator allocates no owned result
    // buffer and requires no destructor, even when returning before the end.
    return selected;
}
inline int CountOwned(const native::RValue* selector,bool first_only=false,native::Instance* self=nullptr,native::Instance* other=nullptr) {
    if(query_begin&&query_peek&&query_advance) {
        if(first_only)return NativeFilteredFind(selector,0,self,other)>=100000?1:0;
        int count=0;NativeFilteredFind(selector,-1,self,other,&count);return count;
    }
    int count=0;std::uint64_t skipped=0;
    VisitOwned(selector,[&](native::Instance*){++count;return !first_only;},&skipped);
    if(skipped)query_filtered.fetch_add(skipped,std::memory_order_relaxed);
    return count;
}
inline const native::RValue* OwnedReadSelector(const native::RValue* selector, native::RValue& temporary) {
    const auto* mapped=RemapAssetSelector(selector,temporary);
    if(mapped!=selector || !NeedsFilter(selector)) return mapped;
    native::Instance* selected=nullptr;
    VisitOwned(selector,[&](native::Instance* instance) {selected=instance;return false;});
    temporary=*selector;
    if(selected) {temporary.kind=6;temporary.pointer=selected;}
    else {temporary.kind=7;temporary.integer=-4;}
    return &temporary;
}
inline bool ReadHook(const native::RValue* selector, int field, std::int32_t index, native::RValue* out, bool array_lvalue, bool prepare_array) {
    native::RValue temporary{};
    const auto* mapped = OwnedReadSelector(selector, temporary);
    if (mapped != selector) read_redirects.fetch_add(1, std::memory_order_relaxed);
    return original_read(mapped, field, index, out, array_lvalue, prepare_array);
}
inline bool ContextReadHook(const native::RValue* selector, int field, std::int32_t index, native::RValue* out, native::Instance* self, native::Instance* other) {
    native::RValue temporary{};
    const auto* mapped = OwnedReadSelector(selector, temporary);
    if (mapped != selector) read_redirects.fetch_add(1, std::memory_order_relaxed);
    return original_context_read(mapped, field, index, out, self, other);
}
inline bool WriteHook(const native::RValue* selector, int field, std::int32_t index, native::RValue* value) {
    native::RValue temporary{};
    const auto* mapped = RemapAssetSelector(selector, temporary);
    if(mapped==selector && NeedsFilter(selector)) {
        bool success=true;
        VisitOwned(selector,[&](native::Instance* instance) {
            native::RValue target{};target.kind=6;target.pointer=instance;
            success=original_write(&target,field,index,value)&&success;
            write_redirects.fetch_add(1,std::memory_order_relaxed);return true;
        });
        return success;
    }
    if (mapped != selector) write_redirects.fetch_add(1, std::memory_order_relaxed);
    return original_write(mapped, field, index, value);
}
inline bool BuiltinWriteHook(const native::RValue* selector, int field, std::int32_t index, native::RValue* value) {
    native::RValue temporary{};
    const auto* mapped = RemapAssetSelector(selector, temporary);
    if(mapped==selector && NeedsFilter(selector)) {
        bool success=true;
        VisitOwned(selector,[&](native::Instance* instance) {
            native::RValue target{};target.kind=6;target.pointer=instance;
            success=original_builtin_write(&target,field,index,value)&&success;
            write_redirects.fetch_add(1,std::memory_order_relaxed);return true;
        });
        return success;
    }
    if (mapped != selector) write_redirects.fetch_add(1, std::memory_order_relaxed);
    return original_builtin_write(mapped, field, index, value);
}
inline int WithHook(void* iterator, native::Instance** self, native::Instance** other, const native::RValue* selector) {
    iterator_filters.erase(iterator);
    native::RValue temporary{};
    const auto* mapped = RemapAssetSelector(selector, temporary);
    if (mapped != selector) with_redirects.fetch_add(1, std::memory_order_relaxed);
    // Let the runner allocate its sentinel and perform its ordinary ID/liveness
    // checks. Its native next/destructor helpers continue to own the iterator.
    int count=original_with(iterator, self, other, mapped);
    if (mapped==selector && NeedsFilter(selector)) {
        iterator_filters.Set(iterator,active_owner);
        while(count>0 && !MatchesOwner(*self,active_owner)) {
            with_filtered.fetch_add(1,std::memory_order_relaxed);
            if (!original_next(iterator,self,other)) count=0;
        }
    }
    return count;
}
inline bool WithNextHook(void* iterator, native::Instance** self, native::Instance** other) {
    const int owner=iterator_filters.Owner(iterator);
    while(original_next(iterator,self,other)) {
        if (owner<0 || !enabled.load(std::memory_order_relaxed) || MatchesOwner(*self,owner)) return true;
        with_filtered.fetch_add(1,std::memory_order_relaxed);
    }
    return false;
}
inline void WithEndHook(void* iterator, native::Instance** self, native::Instance** other) {
    iterator_filters.erase(iterator);
    original_end(iterator,self,other);
}
}

inline bool InstallContextHooks(Aurie::AurieModule* module, std::uintptr_t base, void(*log)(const char*)) {
    detail::logger = log;
    detail::runner_base = base;
    if (detail::installed) return true;
    const unsigned char query_begin_signature[16]={0x40,0x53,0x48,0x83,0xec,0x20,0x0f,0xb6,0x44,0x24,0x50,0x45,0x33,0xdb,0x4c,0x89};
    const unsigned char query_peek_signature[16]={0x48,0x89,0x5c,0x24,0x08,0x8b,0x59,0x2c,0x45,0x33,0xdb,0x4c,0x8b,0xc1,0x45,0x8b};
    const unsigned char query_advance_signature[16]={0x44,0x8b,0x41,0x2c,0x41,0x8d,0x40,0x02,0x83,0xf8,0x05,0x0f,0x87,0x8f,0x00,0x00};
    if(std::memcmp(reinterpret_cast<const void*>(base+0xfc6ef0),query_begin_signature,16)
       ||std::memcmp(reinterpret_cast<const void*>(base+0xfc7130),query_peek_signature,16)
       ||std::memcmp(reinterpret_cast<const void*>(base+0xfc8830),query_advance_signature,16)) {detail::Log("context native query iterator signature mismatch");return false;}
    struct HookSpec { const char* name; std::uintptr_t rva; std::array<unsigned char, 16> signature; void* detour; void** original; };
    const HookSpec hooks[] = {
        {"NovaShip.Context.Read", 0xf17950, {0x48,0x89,0x6c,0x24,0x18,0x48,0x89,0x7c,0x24,0x20,0x41,0x56,0x48,0x83,0xec,0x40}, reinterpret_cast<void*>(detail::ReadHook), reinterpret_cast<void**>(&detail::original_read)},
        {"NovaShip.Context.Write", 0xf17e50, {0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xec,0x20,0x8b}, reinterpret_cast<void*>(detail::WriteHook), reinterpret_cast<void**>(&detail::original_write)},
        {"NovaShip.Context.WriteBuiltin", 0xf170e0, {0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xec,0x20,0x8b}, reinterpret_cast<void*>(detail::BuiltinWriteHook), reinterpret_cast<void**>(&detail::original_builtin_write)},
        {"NovaShip.Context.ReadSelfOther", 0xf17670, {0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xec,0x30,0x8b}, reinterpret_cast<void*>(detail::ContextReadHook), reinterpret_cast<void**>(&detail::original_context_read)},
        {"NovaShip.Context.With", 0xf2d960, {0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57}, reinterpret_cast<void*>(detail::WithHook), reinterpret_cast<void**>(&detail::original_with)},
        {"NovaShip.Context.WithNext", 0xf2e080, {0x48,0x83,0x79,0x10,0x00,0x74,0x17,0x48,0x83,0x41,0x18,0xf8,0x48,0x8b,0x41,0x18}, reinterpret_cast<void*>(detail::WithNextHook), reinterpret_cast<void**>(&detail::original_next)},
        {"NovaShip.Context.WithEnd", 0xf2d630, {0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xec,0x20,0x48}, reinterpret_cast<void*>(detail::WithEndHook), reinterpret_cast<void**>(&detail::original_end)}
    };
    // Validate every target before creating the first hook.
    for (const auto& hook : hooks) {
        if (std::memcmp(reinterpret_cast<const void*>(base + hook.rva), hook.signature.data(), hook.signature.size()) != 0) {
            char message[192]; std::snprintf(message, sizeof(message), "context signature mismatch %s RVA=%llx", hook.name, static_cast<unsigned long long>(hook.rva)); detail::Log(message);
            return false;
        }
    }
    unsigned int created = 0;
    for (const auto& hook : hooks) {
        const auto status = Aurie::MmCreateHook(module, hook.name, reinterpret_cast<void*>(base + hook.rva), hook.detour, hook.original);
        char message[192]; std::snprintf(message, sizeof(message), "context hook %s status=%s", hook.name, Aurie::AurieStatusToString(status)); detail::Log(message);
        if (!Aurie::AurieSuccess(status)) {
            while (created) Aurie::MmRemoveHook(module, hooks[--created].name);
            return false;
        }
        ++created;
    }
    detail::installed = true;
    detail::query_begin=reinterpret_cast<detail::QueryBegin>(base+0xfc6ef0);
    detail::query_peek=reinterpret_cast<detail::QueryPeek>(base+0xfc7130);
    detail::query_advance=reinterpret_cast<detail::QueryAdvance>(base+0xfc8830);
    return true;
}

namespace detail {
inline bool Readable(const void* address, std::size_t bytes) {
    MEMORY_BASIC_INFORMATION info{};
    return address && VirtualQuery(address,&info,sizeof(info)) && info.State==MEM_COMMIT
        && !(info.Protect&(PAGE_NOACCESS|PAGE_GUARD))
        && reinterpret_cast<std::uintptr_t>(address)+bytes<=reinterpret_cast<std::uintptr_t>(info.BaseAddress)+info.RegionSize;
}
inline native::Instance* ResolveId(int id) {
    if (!runner_base || id<100000) return nullptr;
    const auto table=*reinterpret_cast<std::uintptr_t*>(runner_base+0x1a04228);
    const auto mask=*reinterpret_cast<std::uint32_t*>(runner_base+0x1a04230);
    if (!table || mask>0xffffff) return nullptr;
    const auto* bucket=reinterpret_cast<const std::uintptr_t*>(table+static_cast<std::uintptr_t>(mask&static_cast<unsigned>(id))*16);
    if (!Readable(bucket,8)) return nullptr;
    auto node=*bucket;
    for(unsigned n=0;node&&n<4096;++n) {
        if (!Readable(reinterpret_cast<void*>(node),32)) return nullptr;
        if (*reinterpret_cast<int*>(node+16)==id) {
            auto* instance=*reinterpret_cast<native::Instance**>(node+24);
            if (!Readable(instance,0xbc) || (*reinterpret_cast<std::uint32_t*>(reinterpret_cast<std::uintptr_t>(instance)+0xb8)&0x100001)) return nullptr;
            return instance;
        }
        node=*reinterpret_cast<std::uintptr_t*>(node+8);
    }
    return nullptr;
}
inline int InstanceResult(const YYTK::RValue& value) {
    const auto& raw=reinterpret_cast<const native::RValue&>(value);
    const auto kind=raw.kind&0xffffffu;
    if(kind==15||kind==7) return static_cast<std::int32_t>(raw.integer);
    if(kind==0 && raw.real>=INT32_MIN && raw.real<=INT32_MAX) return static_cast<int>(raw.real);
    return -4;
}
inline bool Exists(const native::RValue& selector, YYTK::CInstance* self, YYTK::CInstance* other) {
    YYTK::RValue result;
    native::RValue arg=selector;
    original_exists(result,self,other,1,reinterpret_cast<YYTK::RValue*>(&arg));
    return result.ToBoolean();
}
// Enumerate through the original native find to retain object ancestry and
// engine liveness semantics; registry pointers never substitute for matches.
inline int FilteredFind(const native::RValue& selector, int wanted, YYTK::CInstance* self, YYTK::CInstance* other, int* total=nullptr) {
    if(query_begin&&query_peek&&query_advance)return NativeFilteredFind(&selector,wanted,self,other,total);
    int accepted=0, selected=-4;
    for(int index=0;index<100000;++index) {
        native::RValue args[2]{selector,{}};
        args[1].real=static_cast<double>(index);
        YYTK::RValue result;
        original_find(result,self,other,2,reinterpret_cast<YYTK::RValue*>(args));
        const int id=InstanceResult(result);
        if(id<100000) break;
        if(MatchesOwner(ResolveId(id),active_owner)) {
            if(accepted==wanted) selected=id;
            ++accepted;
            if(!total && selected>=100000) break;
        } else query_filtered.fetch_add(1,std::memory_order_relaxed);
    }
    if(total) *total=accepted;
    return selected;
}
inline void ExistsHook(YYTK::RValue& result, YYTK::CInstance* self, YYTK::CInstance* other, int argc, YYTK::RValue* argv) {
    if(argc<1||!argv) {original_exists(result,self,other,argc,argv);return;}
    const auto* selector=reinterpret_cast<const native::RValue*>(argv);
    native::RValue temporary{};
    const auto* mapped=RemapAssetSelector(selector,temporary);
    if(mapped!=selector) { result=YYTK::RValue(Exists(*mapped,self,other)?1.0:0.0);return; }
    if(NeedsFilter(selector)) {result=YYTK::RValue(CountOwned(selector,true,self,other)>0?1.0:0.0);return;}
    original_exists(result,self,other,argc,argv);
}
inline void FindHook(YYTK::RValue& result, YYTK::CInstance* self, YYTK::CInstance* other, int argc, YYTK::RValue* argv) {
    if(argc<2||!argv) {original_find(result,self,other,argc,argv);return;}
    const auto* selector=reinterpret_cast<const native::RValue*>(argv);
    native::RValue temporary{};
    const auto* mapped=RemapAssetSelector(selector,temporary);
    const int index=argv[1].ToInt32();
    if(mapped!=selector) {
        if(index==0 && Exists(*mapped,self,other)) result=reinterpret_cast<const YYTK::RValue&>(*mapped);
        else result=YYTK::RValue(-4.0);
        return;
    }
    if(NeedsFilter(selector)) {
        const int id=index<0?-4:FilteredFind(*selector,index,self,other);
        if(id<100000) result=YYTK::RValue(-4.0);
        else { native::RValue reference{};reference.kind=15;reference.integer=static_cast<std::int64_t>(0x0400000100000000ull|static_cast<std::uint32_t>(id));result=reinterpret_cast<const YYTK::RValue&>(reference); }
        return;
    }
    original_find(result,self,other,argc,argv);
}
inline void NumberHook(YYTK::RValue& result, YYTK::CInstance* self, YYTK::CInstance* other, int argc, YYTK::RValue* argv) {
    if(argc<1||!argv) {original_number(result,self,other,argc,argv);return;}
    const auto* selector=reinterpret_cast<const native::RValue*>(argv);
    native::RValue temporary{};
    const auto* mapped=RemapAssetSelector(selector,temporary);
    if(mapped!=selector) {result=YYTK::RValue(Exists(*mapped,self,other)?1.0:0.0);return;}
    if(NeedsFilter(selector)) {result=YYTK::RValue(static_cast<double>(CountOwned(selector,false,self,other)));return;}
    original_number(result,self,other,argc,argv);
}
}

// Optional query hooks: call after the SDK interface is ready. Only selector
// queries are intercepted; creation/destroy/broad builtin arguments are untouched.
inline bool InstallQueryHooks(Aurie::AurieModule* module, YYTK::YYTKInterface* api) {
    if(detail::queries_installed) return true;
    if(!api || !detail::installed) return false;
    struct Query { const char* routine; const char* name; YYTK::TRoutine detour; YYTK::TRoutine* original; void* target; };
    Query queries[]={
        {"instance_exists","NovaShip.Context.Exists",detail::ExistsHook,&detail::original_exists,nullptr},
        {"instance_find","NovaShip.Context.Find",detail::FindHook,&detail::original_find,nullptr},
        {"instance_number","NovaShip.Context.Number",detail::NumberHook,&detail::original_number,nullptr}
    };
    // SDK lookup binds each hook to the runner's actual named builtin routine.
    for(auto& query:queries) {
        if(!Aurie::AurieSuccess(api->GetNamedRoutinePointer(query.routine,&query.target)) || !query.target) {
            detail::Log("context query builtin lookup failed");return false;
        }
    }
    unsigned created=0;
    for(auto& query:queries) {
        const auto status=Aurie::MmCreateHook(module,query.name,query.target,reinterpret_cast<void*>(query.detour),reinterpret_cast<void**>(query.original));
        char message[192];std::snprintf(message,sizeof(message),"context query hook %s target=%p status=%s",query.routine,query.target,Aurie::AurieStatusToString(status));detail::Log(message);
        if(!Aurie::AurieSuccess(status)) {while(created) Aurie::MmRemoveHook(module,queries[--created].name);return false;}
        ++created;
    }
    detail::queries_installed=true;
    return true;
}
} // namespace ndship
