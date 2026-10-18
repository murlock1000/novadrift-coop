#include "../src/ship_input_sharing.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace sharing=ndship::input_sharing;
namespace config=ndship::config;
using RValue=novadrift_native::RValue;
static unsigned checks=0;
static void Check(bool value,const char* description) {
    ++checks;if(!value){std::fprintf(stderr,"FAIL: %s\n",description);std::exit(1);}
}
static RValue Real(double number,std::uint32_t kind=0){RValue value{};value.real=number;value.kind=kind;return value;}
static RValue Integer(std::int64_t number,std::uint32_t kind){RValue value{};value.integer=number;value.kind=kind;return value;}

int main() {
    config::Profile profile;profile.keys.fill(-1);
    profile.keys[static_cast<std::size_t>(config::Action::Up)]='W';
    profile.keys[static_cast<std::size_t>(config::Action::Left)]='A';
    profile.keys[static_cast<std::size_t>(config::Action::Down)]='S';
    profile.keys[static_cast<std::size_t>(config::Action::Right)]='D';
    sharing::KeyStorage output{};
    RValue a=Real('A'),s=Real('S'),space=Real(32),mouse1=Real(1),mouse2=Real(2),escape=Real(27);
    RValue* thrust[]={&mouse2,&a};RValue* fire[]={&mouse1,&s};
    auto result=sharing::FilterNativeKeys(profile,2,thrust,output);
    Check(result.Changed()&&result.removed==1&&result.argc==1&&result.argv[0]==&mouse2,"WASD reserves alternate A thrust while preserving right mouse thrust");
    result=sharing::FilterNativeKeys(profile,2,fire,output);
    Check(result.Changed()&&result.argc==1&&result.argv[0]==&mouse1,"WASD reserves alternate S fire while preserving left mouse fire");
    // These two native helper calls represent the same physical keyboard
    // event as P2's down/left input. Neither can reach a P1 keyboard binding.
    RValue* keyboard_only[]={&a,&s};result=sharing::FilterNativeKeys(profile,2,keyboard_only,output);
    Check(result.argc==0&&result.removed==2,"P2 movement alone leaves no P1 native action arguments");
    RValue* mixed[]={&a,&space,&s,&escape,&mouse1,&mouse2};
    result=sharing::FilterNativeKeys(profile,6,mixed,output);
    Check(result.argc==4&&result.argv[0]==&space&&result.argv[1]==&escape&&result.argv[2]==&mouse1&&result.argv[3]==&mouse2,"retained native arguments preserve order and pointer identity");
    Check(a.real=='A'&&s.real=='S',"filter does not mutate native RValues");

    // Cover native numeric layouts, including kind modifier bits and an
    // int32 whose upper union bytes contain unrelated data.
    for(auto value:{Real('A'),Real('A',0x80000000u),Integer('A',7),Integer('A',10),Integer(0x123400000041LL,7)}) {
        RValue* args[]={&value};result=sharing::FilterNativeKeys(profile,1,args,output);
        Check(result.argc==0&&result.removed==1,"real/int32/int64 keyboard forms reserve the same physical key");
    }
    for(auto value:{Real(1),Real(2),Real(3),Real(4),Real(5),Real(6),Real(7),Real(27),Integer(27,7),Integer(27,10)}) {
        profile.keys[0]=static_cast<int>(value.kind==0?value.real:value.integer);
        RValue* args[]={&value};result=sharing::FilterNativeKeys(profile,1,args,output);
        Check(!result.Changed()&&result.argv==args,"mouse and Escape remain P1 even when configured for P2");
    }
    profile.keys[0]='W';
    for(auto value:{Real('A',13),Integer('A',15),Integer('A',5),Real(65.5),Real(std::numeric_limits<double>::quiet_NaN()),Real(std::numeric_limits<double>::infinity()),Real(-1),Integer(0x100000041LL,10)}) {
        RValue* args[]={&value};result=sharing::FilterNativeKeys(profile,1,args,output);
        Check(!result.Changed()&&result.argv==args,"unknown kinds/non-key values retain exact original argument view");
    }
    RValue* null_arg[]={nullptr};result=sharing::FilterNativeKeys(profile,1,null_arg,output);
    Check(!result.Changed()&&result.argv==null_arg,"null arguments are preserved");
    profile.device=config::Device::Controller;result=sharing::FilterNativeKeys(profile,2,keyboard_only,output);
    Check(!result.Changed()&&result.argv==keyboard_only&&result.argc==2,"controller-only P2 leaves every P1 key available");
    profile.device=config::Device::Both;result=sharing::FilterNativeKeys(profile,2,keyboard_only,output);
    Check(result.argc==0&&result.removed==2,"combined keyboard/controller device reserves configured keyboard keys");
    RValue* too_many[17];for(auto& item:too_many)item=&a;
    result=sharing::FilterNativeKeys(profile,17,too_many,output);
    Check(!result.Changed()&&result.argc==17&&result.argv==too_many,"more than16 native varargs fall back without truncation");
    RValue* tiny[1]{};result=sharing::FilterNativeKeys(profile,2,keyboard_only,tiny,1);
    Check(!result.Changed()&&result.argc==2&&result.argv==keyboard_only&&tiny[0]==nullptr,"insufficient output capacity falls back without writing");
    result=sharing::FilterNativeKeys(profile,0,nullptr,output);
    Check(result.argc==0&&result.argv==nullptr&&!result.Changed(),"empty native call remains empty");
    std::printf("PASS %u input-sharing contract checks\n",checks);
}
