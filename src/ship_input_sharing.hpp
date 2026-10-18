#pragma once
// Native P1 input helper argument filtering. The caller enables this only
// while P2 has joined and invokes the original helper with the returned view.
// No allocation, input polling, RValue mutation, or global state changes.
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "native_abi.hpp"
#include "ship_config.hpp"

namespace ndship::input_sharing {
namespace native=novadrift_native;
inline constexpr std::size_t MaxNativeKeys=16;
using KeyStorage=std::array<native::RValue*,MaxNativeKeys>;

struct NativeKeys {
    int argc;
    native::RValue** argv;
    std::size_t removed=0;
    bool Changed()const noexcept{return removed!=0;}
};

inline bool KeyboardEnabled(const config::Profile& profile) noexcept {
    return profile.device==config::Device::Keyboard||profile.device==config::Device::Both;
}
// Reject nonintegral/referenced/unknown values rather than interpreting their
// union payload as a key. Mouse VKs1..7 and Escape always remain available P1.
inline bool KeyboardKey(const native::RValue* value,int& key) noexcept {
    if(!value)return false;
    switch(value->kind&0xffffffu) {
        case 0: {
            const double number=value->real;
            if(!std::isfinite(number)||number<8||number>255||std::trunc(number)!=number)return false;
            key=static_cast<int>(number);break;
        }
        case 7: key=static_cast<std::int32_t>(value->integer);break;
        case 10:
            if(value->integer<8||value->integer>255)return false;
            key=static_cast<int>(value->integer);break;
        default:return false;
    }
    return key>=8&&key<=255&&key!=27;
}
inline bool ReservedByPlayer2(const config::Profile& profile,const native::RValue* value) noexcept {
    if(!KeyboardEnabled(profile))return false;
    int key=0;if(!KeyboardKey(value,key))return false;
    for(int binding:profile.keys)if(binding==key)return true;
    return false;
}

// Unsupported argument counts/capacity fall back to the EXACT original view;
// do not truncate varargs. The caller-owned output must outlive the native
// helper invocation. Retained arguments keep their original RValue pointers.
inline NativeKeys FilterNativeKeys(const config::Profile& profile,int argc,native::RValue** argv,
                                  native::RValue** output,std::size_t output_capacity) noexcept {
    NativeKeys result{argc,argv,0};
    if(!KeyboardEnabled(profile)||argc<=0||argc>static_cast<int>(MaxNativeKeys)||!argv||
       !output||output_capacity<static_cast<std::size_t>(argc))return result;
    int kept=0;
    for(int i=0;i<argc;++i) {
        if(ReservedByPlayer2(profile,argv[i]))++result.removed;
        else output[kept++]=argv[i];
    }
    if(result.Changed()){result.argc=kept;result.argv=output;}
    return result;
}
inline NativeKeys FilterNativeKeys(const config::Profile& profile,int argc,native::RValue** argv,
                                  KeyStorage& output) noexcept {
    return FilterNativeKeys(profile,argc,argv,output.data(),output.size());
}
} // namespace ndship::input_sharing
