// Native two-ship experiment for pinned Nova Drift 1.2.38.4 only.
#define NOMINMAX
#include <windows.h>
#include <Xinput.h>
#include <algorithm>
#include <cstdio>
#include <share.h>
#include <intrin.h>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <YYToolkit/YYTK_Shared.hpp>
#include "native_abi.hpp"
#include "ship_context.hpp"
#include "ship_input.hpp"
#include "ship_build.hpp"
#include "ship_assets.hpp"
#include "ship_upgrade.hpp"
#include "ship_menu_input.hpp"
#include "ship_lifecycle.hpp"
#include "ship_signatures.hpp"
#include "ship_hud.hpp"
#include "ship_perf.hpp"
#include "ship_surface_hud.hpp"
#include "ship_input_sharing.hpp"
#include "ship_balance.hpp"
#include "ship_coop_flow.hpp"
using namespace Aurie;
using namespace YYTK;
namespace nd=novadrift_native;
namespace {
namespace Trays {bool Busy();bool IsOpen(int);bool BlocksInput(int);bool BlocksKey(int,int);bool ConsumesClick(int);void Reset();void Request(int);void Capture(void*,const char*);void Tick();void Draw(void*,double,double);void Trace(int);}
namespace PauseBuild {void Capture(void*,const char*);void Draw(void*,double,double);nd::RValue* Owned(nd::Script,void*,void*,nd::RValue*,int,nd::RValue**);}
YYTKInterface* api=nullptr; AurieModule* module_self=nullptr; nd::Access access{};
TRoutine engine_keyboard_check=nullptr;
ndship::config::LiveProfile p1_controls;bool p1_custom=false;
std::unordered_map<int,RValue> p1_input;
std::filesystem::path root; FILE* log_file=nullptr; FILE* telemetry=nullptr; FILE* performance=nullptr; FILE* presentation=nullptr;
unsigned long long marker_draws[3]{},native_rect_hidden=0,hud_frames=0;
std::unordered_set<int> hidden_tutorial_effects;
int obsolete_tutorial_scope=0;
bool ObsoleteAdvice(void*);
bool ReplacedAdviceText(const RValue&);
bool CompactPresentation();
void RememberTutorialEffects(void*);
bool StockEffectCaptured();
std::string gameplay_help=ndship::config::Help(ndship::config::Profile{}),menu_help=ndship::config::Help(ndship::config::Profile{},true);
ULONGLONG performance_at=0;
int ships[3]{-1,-1,-1}, powers[3]{-1,-1,-1};
std::unordered_map<int,int> owners;
std::map<int,RValue> overlay;
void* globals=nullptr;
using Slot=nd::RValue*(*)(void*,int);
Slot original_global_read=nullptr,original_global_write=nullptr;
nd::Event original_player_create=nullptr,original_powers_create=nullptr,original_player_step=nullptr;
nd::Event original_player_destroy=nullptr,original_player_step2=nullptr;
nd::Script original_gain_level=nullptr,original_restart=nullptr;
using CodeEvent=FunctionWrapper<bool(CInstance*,CInstance*,CCode*,RValue*,int)>;
bool level_award_inflight=false,pause_stats_rendering=false;
bool spawning_powers=false,spawning_player=false,ready=false,joining=false;
int respawn_owner=0;
int controller_id=-1;
struct HealthEffect {int owner=0;double width=1;};
std::unordered_map<int,HealthEffect> health_effects;
ndship::config::LiveProfile controls;
ULONGLONG controls_at=0;
int menu_owner=1; unsigned long long generation=0,steps[3]{},shots[3]{};
ULONGLONG commands_at=0,telemetry_at=0;
bool qa_mode=false,diagnostics=false;
std::atomic<unsigned long long> input_frame{0};
bool p2_ui_valid=false,ui_overlay_active=false,save_native_ui=false;
ULONGLONG hint_until[3]{},upgrade_hint_until=0;
int upgrade_hint_owner=0,ui_primary=1;bool upgrade_was_open=false;
void ResetNativeUI(){hidden_tutorial_effects.clear();health_effects.clear();p2_ui_valid=false;ui_primary=1;hint_until[1]=hint_until[2]=upgrade_hint_until=0;upgrade_was_open=false;upgrade_hint_owner=0;}
double HintOpacity(ULONGLONG until,ULONGLONG now){return until>now?std::min(1.0,(until-now)/1000.0):0.0;}
std::string last_commands;
struct Commands { int counter=0,join=0,thrust=0,left=0,right=0,fire=0,qa_focus=0,augment=0,owner=2,level=0,screen=0,mutate=0,upgrade=0,confirm=0,kill=0,restart=0,xp=0,diagnostics=0,perf=0,key=0,damage=0,configure=0,nativehud=0,mutate_hud=0,engine_input=0,engine_key=0,engine_key2=0,mutate_tray_input=0,mutate_tray_cursor=0,mutate_death_step=0,mutate_thrust_cleanup=0,cursor_fixture=0,cursor_x=0,cursor_y=0,mutate_sharing=0,balance=0,balance_mutate=0,rescue_mode=0,rescue_hold=0,tray=0,tray_action=0,tray_pick=0,collision=0,native_augment=0,tray_fail=0,tray_enemy=0,deferred_upgrade=0,mutate_beacon=0,beacon_edge=0,beacon_center=0,mutate_stock=0,freeze=0,shield_break=0,mutate_healthfx=0,mutate_pause=0,mutate_startup=0; } command;
void Log(const char* t){if(log_file){std::fprintf(log_file,"[%llu] %s\n",GetTickCount64(),t);std::fflush(log_file);}}
bool Readable(std::uintptr_t p,std::size_t n){MEMORY_BASIC_INFORMATION m{};return p>=0x10000&&p<0x7fffffffffffULL&&n<0x10000&&VirtualQuery((void*)p,&m,sizeof m)&&m.State==MEM_COMMIT&&!(m.Protect&(PAGE_NOACCESS|PAGE_GUARD))&&p+n<=(std::uintptr_t)m.BaseAddress+m.RegionSize;}
int Id(void* p){return p&&Readable((std::uintptr_t)p,0xc8)&&*(int*)((char*)p+0x7c)==1?*(int*)((char*)p+0xbc):-1;}
int Asset(void* p){return p&&Readable((std::uintptr_t)p,0xc8)&&*(int*)((char*)p+0x7c)==1?*(int*)((char*)p+0xc0):-1;}
void* Resolve(int id){
 if(id<100000)return nullptr;
 auto table=*(std::uintptr_t*)(access.base+0x1a04228);auto mask=*(unsigned*)(access.base+0x1a04230);
 if(mask>0xffffff||!Readable(table+(mask&id)*16,16))return nullptr;
 auto node=*(std::uintptr_t*)(table+(mask&id)*16);
 for(int i=0;node&&i<1024;++i){if(!Readable(node,32))return nullptr;if(*(int*)(node+16)==id){auto p=*(std::uintptr_t*)(node+24);return Readable(p,0xc8)&&!(*(unsigned*)(p+0xb8)&0x100001)?(void*)p:nullptr;}node=*(std::uintptr_t*)(node+8);}return nullptr;
}
double Read(void* p,std::uintptr_t cell,double fallback=-999999){double v;return p&&access.get(p,cell,v)?v:fallback;}
RValue Ref(int id){ RValue r; r.m_Kind=VALUE_REF;r.m_i64=0x0400000100000000ULL|(unsigned)id;return r; }
RValue Builtin(const char* name,std::vector<RValue> args={},void* self=nullptr){RValue out;auto status=api->CallBuiltinEx(out,name,(CInstance*)self,(CInstance*)self,args);if(!AurieSuccess(status)){char t[200];std::snprintf(t,sizeof t,"builtin %s failed %s",name,AurieStatusToString(status));Log(t);}return out;}
int Owner(void* p){if(!p)return 0;auto found=owners.find(Id(p));if(found!=owners.end())return found->second;return 0;}
void Tag(void* p,int owner){int id=Id(p);if(owner>0&&id>=100000){owners[id]=owner;ndship::RegisterOwnedInstance(p,owner,id);}}
bool P1InputActive(){return p1_custom&&!ndship::upgrade::SessionActive()&&Read(Resolve(controller_id),0x1b004b8,1)==0&&Read(Resolve(controller_id),0x1af7718,0)==0;}
nd::RValue* GlobalRead(void* object,int field){if(object==globals&&ndship::active_owner==1&&P1InputActive()){auto it=p1_input.find(field);if(it!=p1_input.end())return (nd::RValue*)&it->second;}if(object==globals&&ndship::active_owner==2){auto it=overlay.find(field);if(it!=overlay.end()){if(pause_stats_rendering){bool independent=false;for(auto f:ndship::build::IndependentGlobals)if(access.id(f.cell)==field)independent=true;if(!independent)return original_global_read(object,field);}return (nd::RValue*)&it->second;}}return original_global_read(object,field);}
nd::RValue* GlobalWrite(void* object,int field){if(object==globals&&ndship::active_owner==1&&P1InputActive()){auto it=p1_input.find(field);if(it!=p1_input.end())return (nd::RValue*)&it->second;}if(object==globals&&ndship::active_owner==2){auto it=overlay.find(field);if(it!=overlay.end())return (nd::RValue*)&it->second;}return original_global_write(object,field);}
void SetOverlay(std::uintptr_t cell,double value){overlay[access.id(cell)]=RValue(value);}
void CopyOverlay(std::uintptr_t cell){int field=access.id(cell);overlay[field]=*(RValue*)original_global_read(globals,field);}
int UIPos(){auto ui=Builtin("instance_find",{RValue(828),RValue(0)});int id=ui.m_Kind==VALUE_REF?(int)ui.m_i64:-1;return (int)Read(Resolve(id),0x1afa238,-9);}
int UIConfirmable(){auto ui=Builtin("instance_find",{RValue(828),RValue(0)});int id=ui.m_Kind==VALUE_REF?(int)ui.m_i64:-1;return Read(Resolve(id),0x1b004a8,0)!=0;}
double Global(std::uintptr_t cell){return ((RValue*)GlobalRead(globals,access.id(cell)))->ToDouble();}
void SetGlobal(std::uintptr_t cell,double value){*(RValue*)GlobalWrite(globals,access.id(cell))=RValue(value);}
RValue Field(void*p,std::uintptr_t cell);
#include "ship_balance_runtime.hpp"
bool Respawn(int owner,double x,double y);
#include "ship_coop_runtime.hpp"
void ConfigureOverlay(){
 for(auto field:ndship::build::IndependentGlobals)CopyOverlay(field.cell);
 ndship::input::ConfigureInputOverlay([](std::uintptr_t c,nd::RValue v){SetOverlay(c,v.real);});
 ndship::menu_input::ConfigureMenuInputOverlay([](std::uintptr_t c,nd::RValue v){SetOverlay(c,v.real);});
 SetOverlay(0x1b000a8,0);SetOverlay(0x1b01038,0);SetOverlay(0x1b00178,0);
 SetOverlay(0x1afc788,'N');SetOverlay(0x1afc598,-1);SetOverlay(0x1afc7e8,'I');SetOverlay(0x1afc6e8,-1);
 SetOverlay(0x1afc678,'J');SetOverlay(0x1afc688,'L');SetOverlay(0x1afc718,-1);
 SetOverlay(0x1b002e8,0);SetOverlay(0x1b00348,0);
}
void InitializeGlobals(){
 if(original_global_read){globals=*(void**)(access.base+0x1b1c8d0);return;}
 globals=*(void**)(access.base+0x1b1c8d0); if(!globals||!Readable((uintptr_t)globals,8))return;
 auto vt=*(void***)globals;char t[400];std::snprintf(t,sizeof t,"global=%p vtable=%p read RVA=%llx write RVA=%llx",globals,vt,(uintptr_t)vt[1]-access.base,(uintptr_t)vt[2]-access.base);Log(t);
 if(!ndship::signatures::Matches(access.base,(uintptr_t)vt[1]-access.base)||!ndship::signatures::Matches(access.base,(uintptr_t)vt[2]-access.base)){Log("REFUSING global hooks: pinned signatures differ");return;}
 auto a=MmCreateHook(module_self,"Ship.GlobalRead",vt[1],(void*)GlobalRead,(void**)&original_global_read);
 auto b=MmCreateHook(module_self,"Ship.GlobalWrite",vt[2],(void*)GlobalWrite,(void**)&original_global_write);
 std::snprintf(t,sizeof t,"global hooks %s %s",AurieStatusToString(a),AurieStatusToString(b));Log(t);
}
void CommandsRead(bool startup=false){
 if(!startup&&!qa_mode)return;
 auto now=GetTickCount64();if(now-commands_at<40)return;commands_at=now;
 std::ifstream f(root/L"ship-control.txt");std::string text((std::istreambuf_iterator<char>(f)),{});if(text.empty()||text==last_commands)return;last_commands=text;
 Commands c;std::istringstream lines(text);std::string line;
 while(std::getline(lines,line)){std::replace(line.begin(),line.end(),'=',' ');std::istringstream e(line);std::string key;int value;if(!(e>>key>>value))continue;
 #define KEY(k) if(key==#k)c.k=value;
 KEY(counter) KEY(join) KEY(thrust) KEY(left) KEY(right) KEY(fire) KEY(qa_focus) KEY(augment) KEY(owner) KEY(level) KEY(screen) KEY(mutate) KEY(upgrade) KEY(confirm) KEY(kill) KEY(restart) KEY(xp) KEY(diagnostics) KEY(perf) KEY(key) KEY(damage) KEY(configure) KEY(nativehud) KEY(mutate_hud) KEY(engine_input) KEY(engine_key) KEY(engine_key2) KEY(mutate_tray_input) KEY(mutate_tray_cursor) KEY(mutate_death_step) KEY(mutate_thrust_cleanup) KEY(cursor_fixture) KEY(cursor_x) KEY(cursor_y) KEY(mutate_sharing) KEY(balance) KEY(balance_mutate) KEY(rescue_mode) KEY(rescue_hold) KEY(tray) KEY(tray_action) KEY(tray_pick) KEY(collision) KEY(native_augment) KEY(tray_fail) KEY(tray_enemy) KEY(deferred_upgrade) KEY(mutate_beacon) KEY(beacon_edge) KEY(beacon_center) KEY(mutate_stock) KEY(freeze) KEY(shield_break) KEY(mutate_healthfx) KEY(mutate_pause) KEY(mutate_startup)
 #undef KEY
 }
 if(qa_mode&&api){
  const int before[]={command.engine_key,command.engine_key2},after[]={c.engine_key,c.engine_key2};
  for(int key:before)if(key>=8&&key<=255&&std::find(std::begin(after),std::end(after),key)==std::end(after))Builtin("keyboard_key_release",{RValue(key)});
  for(int key:after)if(key>=8&&key<=255&&std::find(std::begin(before),std::end(before),key)==std::end(before))Builtin("keyboard_key_press",{RValue(key)});
 }
 command=c;ndship::SetEnabled(!c.mutate);
}
std::vector<int> OwnedIds(int owner);
RValue Field(void*p,std::uintptr_t cell){RValue result;if(p)((nd::GetValue)(access.base+0xf16590))(p,access.id(cell),nd::Scalar,(nd::RValue*)&result,false,false);return result;}
void Telemetry(){
 auto now=GetTickCount64();if(!telemetry||Trays::Busy()||joining||ndship::lifecycle::IsTearingDown()||level_award_inflight||now-telemetry_at<100)return;telemetry_at=now;
 CoopBalance::State(command.counter);
 CoopFlow::Trace(command.counter);Trays::Trace(command.counter);
 auto p1=Resolve(ships[1]),p2=Resolve(ships[2]),b1=Resolve(powers[1]),b2=Resolve(powers[2]);auto stats=ndship::GetStats();
 std::fprintf(telemetry,"{\"time\":%llu,\"command\":%d,\"generation\":%llu,\"ready\":%d,\"p1\":%d,\"p2\":%d,\"b1\":%d,\"b2\":%d,\"p1_asset\":%d,\"p2_asset\":%d,\"x1\":%.6f,\"y1\":%.6f,\"x2\":%.6f,\"y2\":%.6f,\"angle1\":%.6f,\"angle2\":%.6f,\"vx1\":%.6f,\"vy1\":%.6f,\"vx2\":%.6f,\"vy2\":%.6f,\"hull1\":%.6f,\"hull2\":%.6f,\"rof1\":%.6f,\"rof2\":%.6f,\"steps1\":%llu,\"steps2\":%llu,\"reads\":%llu,\"withs\":%llu}\n",now,command.counter,generation,ready,p1?ships[1]:-1,p2?ships[2]:-1,b1?powers[1]:-1,b2?powers[2]:-1,Asset(p1),Asset(p2),Read(p1,nd::Fields::X),Read(p1,nd::Fields::Y),Read(p2,nd::Fields::X),Read(p2,nd::Fields::Y),Read(p1,nd::Fields::Angle),Read(p2,nd::Fields::Angle),Read(p1,nd::Fields::HSpeed),Read(p1,nd::Fields::VSpeed),Read(p2,nd::Fields::HSpeed),Read(p2,nd::Fields::VSpeed),Read(b1,nd::Fields::Hull),Read(b2,nd::Fields::Hull),Read(b1,0x1aef608),Read(b2,0x1aef608),steps[1],steps[2],stats.read_redirects,stats.with_redirects);std::fflush(telemetry);
 if(qa_mode&&ready&&globals){ndship::OwnerScope one(1);auto choice1=Global(0x1af8ce8),level=Global(0x1af9148);ndship::OwnerScope two(2);auto choice2=Global(0x1af8ce8);
 std::fprintf(telemetry,"{\"time\":%llu,\"command\":%d,\"metrics\":1,\"choices1\":%.6f,\"choices2\":%.6f,\"level\":%.6f,\"shots1\":%llu,\"shots2\":%llu,\"mods1\":%.0f,\"mods2\":%.0f,\"ui\":%d,\"menu_owner\":%d,\"menu_session\":%d,\"xp\":%.6f,\"maxxp\":%.6f,\"weapon1\":%d,\"weapon2\":%d,\"owned1\":%llu,\"owned2\":%llu,\"maxhull1\":%.6f,\"maxhull2\":%.6f,\"context_enabled\":%d,\"confirmable\":%d,\"input_frame\":%llu,\"ui_pos\":%d,\"paused\":%.0f,\"game_over\":%.0f,\"end_score_instances\":%d}\n",now,command.counter,choice1,choice2,level,shots[1],shots[2],b1?(double)Builtin("ds_list_size",{Field(b1,0x1af7678)}).ToInt32():-1,b2?(double)Builtin("ds_list_size",{Field(b2,0x1af7678)}).ToInt32():-1,Builtin("instance_number",{RValue(828)}).ToInt32(),ndship::upgrade::CurrentMenuOwner(),ndship::upgrade::SessionActive(),Read(Resolve(controller_id),0x1a9da98),Read(Resolve(controller_id),0x1afd8e8),b1?(int)Field(b1,0x1af7898).m_i64:-1,b2?(int)Field(b2,0x1af7898).m_i64:-1,OwnedIds(1).size(),OwnedIds(2).size(),Read(b1,0x1af7b78),Read(b2,0x1af7b78),!command.mutate,UIConfirmable(),input_frame.load(std::memory_order_relaxed),UIPos(),Read(Resolve(controller_id),0x1b004b8),Read(Resolve(controller_id),0x1af7718),Builtin("instance_number",{RValue(398)}).ToInt32());std::fflush(telemetry);}
}
void PlayerCreate(void* self,void* other){
 int owner=respawn_owner?respawn_owner:(spawning_player?2:1);
 if(owner==1&&!respawn_owner){CoopFlow::Reset();Trays::Reset();ResetNativeUI();if(powers[2]>=100000&&!ndship::lifecycle::IsTearingDown())ndship::lifecycle::TeardownOwner(2,ndship::lifecycle::Reason::CanonicalRespawn);ndship::upgrade::Reset();ships[2]=-1;powers[2]=-1;ready=false;owners.clear();ndship::ClearOwnedInstances();ndship::ClearBindings();if(Resolve(powers[1])){Tag(Resolve(powers[1]),1);ndship::Bind(1,621,powers[1]);ndship::Bind(0,621,powers[1]);}steps[1]=steps[2]=0;++generation;}
 ships[owner]=Id(self);Tag(self,owner);ndship::Bind(owner,603,Id(self));if(owner==1)ndship::Bind(0,603,Id(self));
 ndship::OwnerScope scope(owner);InitializeGlobals();original_player_create(self,other);
 char t[150];std::snprintf(t,sizeof t,"native Player Create owner=%d id=%d asset=%d",owner,Id(self),Asset(self));Log(t);
}
void PowersCreate(void* self,void* other){
 int owner=spawning_powers?2:1;powers[owner]=Id(self);Tag(self,owner);ndship::Bind(owner,621,Id(self));if(owner==1)ndship::Bind(0,621,Id(self));
 if(owner==2){ndship::lifecycle::TrackBlankPowers(Id(self));Log("P2 Powers created blank; native world initialization suppressed");return;}
 ndship::OwnerScope scope(owner);original_powers_create(self,other);
 char t[130];std::snprintf(t,sizeof t,"native Powers Create owner=%d id=%d",owner,Id(self));Log(t);
}
void Script(std::uintptr_t rva,void* self){nd::RValue result{};result.kind=5;((nd::Script)(access.base+rva))(self,self,&result,0,nullptr);}
bool Respawn(int owner,double x,double y){
 if(owner<1||owner>2||Resolve(ships[owner])||!Resolve(powers[owner]))return false;
 joining=true;ndship::OwnerScope scope(owner);auto build=Resolve(powers[owner]);respawn_owner=owner;
 Builtin("instance_create_depth",{RValue(x),RValue(y),RValue(0.0),RValue(603)});respawn_owner=0;
 auto ship=Resolve(ships[owner]);if(!ship){joining=false;Log("Native revive creation failed");return false;}
 for(auto cell:{0x1afc928ULL,0x1afc8e8ULL,0x1afc8d8ULL})access.set(build,cell,-4);
 Script(0x5fe60,build);Script(0x4c79f0,build);
 const double maximum=Read(build,0x1af7b78,0),effective=Read(build,0x1aeef08,0),hull=effective*.5;
 if(maximum>0&&effective>0){access.set(build,nd::Fields::Hull,hull);access.set(build,0x1aedbc8,hull/maximum);access.set(build,0x1aedc98,hull/maximum);}
 access.set(build,0x1af7fd8,0);access.set(build,0x1aedb98,0);access.set(build,0x1af7f98,std::max(180.0,Read(build,0x1af7f98,0)));
 for(auto cell:{nd::Fields::X,(std::uintptr_t)0x1af4b78})access.set(ship,cell,x);
 for(auto cell:{nd::Fields::Y,(std::uintptr_t)0x1af4b68})access.set(ship,cell,y);
 access.set(ship,nd::Fields::HSpeed,0);access.set(ship,nd::Fields::VSpeed,0);
 if(owner==2)p2_ui_valid=false;
 joining=false;CoopFlow::rescues[owner].Reset();hint_until[owner]=GetTickCount64()+2500;Log("Native teammate revive: preserved build, half hull, temporary protection");return true;
}
void Join(){
 if(ready&&!Resolve(ships[2])&&Resolve(powers[2])&&Resolve(ships[1])){
  if(qa_mode)Respawn(2,Read(Resolve(ships[1]),nd::Fields::X)+70,Read(Resolve(ships[1]),nd::Fields::Y));return;
 }
 if(!original_global_read||!original_global_write)return;
 if(ready||joining||!Resolve(ships[1])||!Resolve(powers[1]))return;joining=true;Log("join begin");
 ConfigureOverlay();ndship::BindEmpty(2,741);
 ndship::OwnerScope scope(2);
 spawning_powers=true;auto made=Builtin("instance_create_depth",{RValue(0.0),RValue(0.0),RValue(0.0),RValue(621)});spawning_powers=false;
 auto src=Resolve(powers[1]),dst=Resolve(powers[2]);if(!src||!dst){Log("join failed missing Powers");ndship::lifecycle::TeardownOwner(2,ndship::lifecycle::Reason::JoinFailure);joining=false;return;}
 auto names=Builtin("variable_instance_get_names",{Ref(powers[1])});int length=Builtin("array_length",{names}).ToInt32();
 char t[220];std::snprintf(t,sizeof t,"clone custom names kind=%d count=%d src=%d dst=%d",names.m_Kind,length,powers[1],powers[2]);Log(t);
 const char* listnames[]={"gearShields","gearBodies","gearWeapons","mods","wildMods","wildModsRare","wildModsVeryRare","playerMods","banishMods","previousMods"};
 for(int i=0;i<length;++i){auto name=names[(size_t)i];auto value=Builtin("variable_instance_get",{Ref(powers[1]),name});std::string key=name.ToString();
  bool list=false;for(auto n:listnames)if(key==n){auto fresh=Builtin("ds_list_create");ndship::lifecycle::TrackPowersList(powers[2],fresh.ToInt32());Builtin("ds_list_copy",{fresh,value});value=fresh;list=true;break;}
  if(!list&&(value.IsArray()||value.m_Kind==VALUE_OBJECT))value=Builtin("variable_clone",{value});
  if(value.m_Kind==VALUE_REF&&((unsigned long long)value.m_i64>>32)==0x04000001ULL){auto linked=Resolve((int)value.m_i64);if(linked&&Owner(linked)==1)value=RValue(-4.0);}
  if(key=="shieldBase"||key=="shieldIndicator"||key=="shieldExtra")value=RValue(-4.0);
  Builtin("variable_instance_set",{Ref(powers[2]),name,value});
 }
 ndship::lifecycle::MarkPowersInitialized(powers[2]);
 for(int i=0;i<12;++i){double alarm=-1;if(access.get(src,nd::Fields::Alarm,alarm,i))access.set(dst,nd::Fields::Alarm,alarm,i);}
 spawning_player=true;auto ship=Builtin("instance_create_depth",{RValue(Read(Resolve(ships[1]),nd::Fields::X)+70),RValue(Read(Resolve(ships[1]),nd::Fields::Y)),RValue(0.0),RValue(603)});spawning_player=false;
 auto p2=Resolve(ships[2]);if(p2){access.set(p2,nd::Fields::X,Read(Resolve(ships[1]),nd::Fields::X)+70);access.set(p2,nd::Fields::Y,Read(Resolve(ships[1]),nd::Fields::Y));}
 if(p2){Script(0x5fe60,dst);for(int owner=1;owner<=2;++owner){auto ship=Resolve(ships[owner]);if(ship){RValue color(ndship::hud::PlayerColor(owner));api->SetBuiltin("image_blend",(CInstance*)ship,NULL_INDEX,color);}}}
 if(p2){double x=Read(Resolve(ships[1]),nd::Fields::X)+70,y=Read(Resolve(ships[1]),nd::Fields::Y);
  for(auto cell:{nd::Fields::X,(std::uintptr_t)0x1af4b78})access.set(p2,cell,x);
  for(auto cell:{nd::Fields::Y,(std::uintptr_t)0x1af4b68})access.set(p2,cell,y);
  access.set(p2,nd::Fields::HSpeed,0);access.set(p2,nd::Fields::VSpeed,0);
 }
 ready=p2!=nullptr;
 if(ready){hint_until[1]=hint_until[2]=GetTickCount64()+4500;SetGlobal(0x1afde48,1);for(int owner=1;owner<=2;++owner){ndship::OwnerScope pick(owner);if(Global(0x1af8ce8)>0&&Global(0x1af8938)<4)Trays::Request(owner);}}
 if(ready)for(int owner=1;owner<=2;++owner)for(int id:OwnedIds(owner)){auto visual=Resolve(id);if(ndship::hud::IsShipVisualAsset(Asset(visual)))ndship::hud::TintShipVisual(api,(CInstance*)visual,owner);}
 if(!p2)ndship::lifecycle::TeardownOwner(2,ndship::lifecycle::Reason::JoinFailure);joining=false;Log(ready?"join complete: independent native ship/build":"join failed ship missing");
}
void PlayerStep(void* self,void* other){
 int owner=Owner(self);ndship::perf::Span measure(owner==2?ndship::perf::Metric::Player2Step:ndship::perf::Metric::Player1Step);ndship::OwnerScope scope(owner);++steps[owner];original_player_step(self,other);
 // Keep the native entry animation. All upgrade entry points are buffered
 // until ready, so P1 cannot spend a starter choice before P2 is cloned.
 if(owner==1){CommandsRead();if((!ready&&steps[1]>=90&&Read(Resolve(powers[1]),0x1af7498,0)>0)||command.join||((GetAsyncKeyState(VK_F8)&1)!=0)){command.join=0;Join();}Telemetry();}
}
void PlayerStep2(void* self,void* other){
 int owner=Owner(self);ndship::OwnerScope scope(owner);original_player_step2(self,other);
 if(ready&&owner>0&&Resolve(ships[owner])==self)ndship::hud::TintShipVisual(api,(CInstance*)self,owner);
}
void PlayerDestroy(void* self,void* other){int owner=Owner(self);ndship::OwnerScope scope(owner);if(owner>0&&!ndship::lifecycle::IsTearingDown()){CoopFlow::Down(owner,self);ndship::lifecycle::TeardownOwner(owner,ndship::lifecycle::Reason::ShipDeath,Id(self));}if(ndship::lifecycle::ShouldRunNativePlayerDestroy(owner,Resolve(ships[owner==2?1:2])!=nullptr))original_player_destroy(self,other);Log("native Player Destroy: global effects suppressed while partner survives");}
TRoutine original_exists=nullptr,original_focus=nullptr;

void Focus(RValue& r,CInstance* s,CInstance* o,int n,RValue* a){if(command.qa_focus){r=RValue(true);return;}original_focus(r,s,o,n,a);}
constexpr const char* InputBuiltins[]={"gamepad_is_connected","gamepad_axis_value","gamepad_button_check","gamepad_button_check_pressed","gamepad_button_check_released","gamepad_button_value","gamepad_set_axis_deadzone","mouse_check_button_pressed","device_mouse_x_to_gui","device_mouse_y_to_gui"};
TRoutine original_input_builtin[10]{};
template<int I> void InputBuiltin(RValue&r,CInstance*s,CInstance*o,int n,RValue*a){
 if(pause_stats_rendering){ndship::OwnerScope canonical(1);original_input_builtin[I](r,s,o,n,a);return;}
 bool blocked=false;
 if constexpr(I<7)blocked=(ndship::active_owner==1&&P1InputActive())||ndship::active_owner==2||(n>0&&a&&controls.Get().device!=ndship::config::Device::Keyboard&&a[0].ToInt32()==controls.Get().controller_slot);
 else blocked=ndship::active_owner==2&&ndship::upgrade::SessionActive();
 if(blocked){r=RValue(I>=8?-10000.0:0.0);return;}original_input_builtin[I](r,s,o,n,a);
}
bool HasShip(int owner);
void Stimuli();
void CodeCallback(CodeEvent& event){
 ndship::perf::Span measure(ndship::perf::Metric::CodeCallback);
 if(qa_mode)CommandsRead();
 auto [self,other,code,result,flags]=event.Arguments();
 int self_id=-1,asset=-1;
 if(self&&*(int*)((char*)self+0x7c)==1){self_id=*(int*)((char*)self+0xbc);asset=*(int*)((char*)self+0xc0);}
 const auto found=owners.find(self_id);
 const int self_owner=found==owners.end()?0:found->second;
 int owner=self_owner?self_owner:ndship::active_owner;
 if(!owner&&other!=self)owner=Owner(other);
 const char* name=code?code->GetName():nullptr;
 Trays::Capture(self,name);
 PauseBuild::Capture(self,name);
 if(asset==377&&name&&std::strstr(name,"_Step_0")){
  if(ndship::upgrade::SessionActive())owner=ndship::upgrade::CurrentMenuOwner();
  else if(ready&&!HasShip(1)&&HasShip(2))owner=2;
 }
 if(asset==377&&name&&std::strstr(name,"_Create_"))controller_id=self_id;
 if(asset==828&&ndship::upgrade::SessionActive())owner=ndship::upgrade::CurrentMenuOwner();
 if(!self_owner&&owner&&self_id>=100000&&(ndship::assets::IsTaggable(asset)||asset==828)){
  owners[self_id]=owner;ndship::RegisterOwnedInstance(self,owner,self_id);
 }
 if(!owner&&name&&(std::strstr(name,"oUpgrade_")||std::strstr(name,"oLoadout_")))owner=ndship::upgrade::CurrentMenuOwner();
 int attention=0;
 if(name&&(std::strstr(name,"_Step_")||std::strstr(name,"_Alarm_")||std::strstr(name,"_Create_")))attention=CoopBalance::Select(self);
 ndship::OwnerScope scope(owner);
 // Old starter tutorials may still be fading when combat begins. Hide only
 // matched presentation instances, keeping their native Step/cleanup alive.
 const bool obsolete_generic=self&&name&&std::strstr(name,"gml_Object_oTutorialGeneric")&&
  (std::strstr(name,"_Step_")||std::strstr(name,"_Alarm_")||std::strstr(name,"_Other_"))&&ReplacedAdviceText(Field(self,0x1b01048));
 if(asset==571&&obsolete_tutorial_scope&&name&&std::strstr(name,"_Create_"))hidden_tutorial_effects.insert(self_id);
 if(obsolete_generic)++obsolete_tutorial_scope;
 // QA captures compare native surfaces with presented pixels. Hold native
 // simulation events while drawing continues, avoiding a recharge mask changing
 // between the surface export and the OS client capture. Normal play never uses it.
 const bool frozen=qa_mode&&command.freeze&&name&&(std::strstr(name,"_Step_")||std::strstr(name,"_Alarm_")||std::strstr(name,"_Collision_")||std::strstr(name,"_Other_"));
 // Native Powers Step2 checks death first, then continues reading Player.
 // In co-op gameOver remains false while a partner survives. Run the same
 // native check before entering that event so its remaining code cannot use
 // a ship destroyed by the check. Last Stand and native protection still run.
 if(!(qa_mode&&command.mutate_death_step)&&!frozen&&ready&&!joining&&!ndship::lifecycle::IsTearingDown()&&asset==621&&
    owner>=1&&owner<=2&&self_id==powers[owner]&&HasShip(owner)&&name&&
    std::strstr(name,"_Step_2")){
  nd::RValue ignored{};ignored.kind=5;
  ((nd::Script)(access.base+0xd67a0))(Resolve(ships[owner]),self,&ignored,0,nullptr);
 }
 // A downed pilot retains Powers for revival, but its native combat events
 // can dereference the destroyed ship (for example Salvo/drone Step2 paths).
 // Resume those events with the same build as soon as its new ship exists.
 const bool downed_build=ready&&!joining&&!ndship::lifecycle::IsTearingDown()&&
  asset==621&&owner>=1&&owner<=2&&self_id==powers[owner]&&!HasShip(owner)&&name&&
  (std::strstr(name,"_Step_")||std::strstr(name,"_Alarm_"));
 {ndship::TargetScope target(attention);if(frozen||downed_build)event.Override(true);else event.Call();}
 if(name&&std::strcmp(name,"gml_Object_oBackground_Draw_0")==0)CoopFlow::DrawBackground(self,input_frame.load(std::memory_order_relaxed));
 // Track associations even before joining. The native tutorial can disappear
 // before its independent pulse finishes, so retain its child's exact ID.
 const bool upgrade_advice=asset==818||asset==819||asset==822||asset==823;
 if(upgrade_advice&&Resolve(self_id)==self){
  RememberTutorialEffects(self);
  // These are native Banish/Decline/Reroll/Upgrade guides. Decline and
  // Reroll have a default sTutorialBarWide sprite separate from their text.
  // Keep generic encounter warnings and all tutorial simulation/cleanup.
  if(CompactPresentation()){RValue hidden(false);api->SetBuiltin("visible",self,NULL_INDEX,hidden);}
 }
 if(obsolete_generic){--obsolete_tutorial_scope;if(CompactPresentation()&&Resolve(self_id)==self){RValue hidden(false);api->SetBuiltin("visible",self,NULL_INDEX,hidden);}}
 if(asset==377&&name&&std::strstr(name,"_Step_0")){if(p1_custom){if(p1_controls.Get().mouse_aim)ndship::Unbind(1,741);else ndship::BindEmpty(1,741);}if(!frozen)CoopFlow::Tick();Trays::Tick();}
 if(ready&&owner&&name){
  if(ndship::hud::IsShipVisualAsset(asset)&&std::strstr(name,"_Create_"))ndship::hud::TintShipVisual(api,self,owner);

 }
 if(name&&(std::strstr(name,"_Destroy_")||std::strstr(name,"_CleanUp_"))){hidden_tutorial_effects.erase(self_id);health_effects.erase(self_id);owners.erase(self_id);ndship::UnregisterOwnedInstance(self);CoopBalance::targets.erase(self_id);}
 if(qa_mode)Telemetry();
 if(ready&&!joining&&!Trays::Busy()&&!ndship::lifecycle::IsTearingDown()){
  if(qa_mode)Stimuli();
  ndship::upgrade::ProcessPending();
 }
}

ndship::input::KeyboardState keyboard,p1_keyboard;
nd::Script original_held=nullptr,original_pressed=nullptr,original_released=nullptr,original_pad_held=nullptr,original_pad_pressed=nullptr,original_end_score=nullptr;
void ReloadControls(){
 auto now=GetTickCount64();if(controls_at&&now-controls_at<1000)return;controls_at=now;
 std::string first_error;if(p1_controls.ReloadIfChanged(root/ndship::config::PlayerOneFileName,first_error)){
  p1_custom=true;p1_keyboard.Reset();p1_input.clear();
  ndship::input::ConfigureInputOverlay([](std::uintptr_t cell,nd::RValue value){p1_input[access.id(cell)]=RValue(value.real);});
  p1_input[access.id(0x1b00068)]=RValue(p1_controls.Get().mouse_aim?0:1);Log("P1 co-op control profile loaded");
 }else if(!first_error.empty())Log(first_error.c_str());
 std::string error;if(controls.ReloadIfChanged(root/ndship::config::FileName,error)){gameplay_help=ndship::config::Help(controls.Get());menu_help=ndship::config::Help(controls.Get(),true);if(ready)hint_until[2]=GetTickCount64()+4500;keyboard.Reset();Log("P2 control profile loaded");}
 else if(!error.empty())Log(error.c_str());
}
void SampleKeyboard(int owner=2){
 auto& buttons=owner==1?p1_keyboard:keyboard;
 static unsigned long long sampled[3]={~0ULL,~0ULL,~0ULL};
 auto tick=input_frame.load(std::memory_order_relaxed);if(sampled[owner]==tick)return;sampled[owner]=tick;
 buttons.SetSelectedConstruct((int)Read(Resolve(powers[owner]),0x1aee7e8,1));
 if(owner==2&&qa_mode&&!command.key&&!command.engine_input){buttons.SampleBot(tick,command.thrust!=0,command.right-command.left,(command.fire||command.confirm)!=0);return;}
 if(!qa_mode&&!Builtin("window_has_focus").ToBoolean()){buttons.Update(tick,0);return;}
 const auto& profile=owner==1?p1_controls.Get():controls.Get();ndship::input::ControllerFrame pad;
 if(!qa_mode&&profile.device!=ndship::config::Device::Keyboard){
  XINPUT_STATE state{};if(XInputGetState(profile.controller_slot,&state)==ERROR_SUCCESS){
   pad.connected=true;pad.x=state.Gamepad.sThumbLX/32768.0;pad.y=-state.Gamepad.sThumbLY/32768.0;
   pad.buttons=state.Gamepad.wButtons;
   if(state.Gamepad.bLeftTrigger>30)pad.buttons|=ndship::config::LeftTrigger;
   if(state.Gamepad.bRightTrigger>30)pad.buttons|=ndship::config::RightTrigger;
  }
 }
 auto engine_down=[](int key){
  if(key<1||key>255)return false;
  if(key<8){int mouse=key==1?1:key==2?2:key==4?3:key==5?4:key==6?5:-1;return mouse>0&&Builtin("mouse_check_button",{RValue(mouse)}).ToBoolean();}
  if(!engine_keyboard_check)return false;
  RValue value,arg(key);engine_keyboard_check(value,nullptr,nullptr,1,&arg);return value.ToBoolean();
 };
 auto down=[&](int key){
  if(key==VK_LBUTTON&&Trays::ConsumesClick(owner))return false;
  if(qa_mode)return command.engine_input?engine_down(key):key==command.key;
  return (GetAsyncKeyState(key)&0x8000)!=0||engine_down(key);
 };
 if(ndship::upgrade::SessionActive())buttons.SampleConfiguredMenu(tick,profile,down,pad);
 else buttons.SampleConfigured(tick,profile,down,Read(Resolve(ships[owner]),nd::Fields::Angle,0),pad);
}

nd::RValue* Input(nd::Script original,ndship::input::Edge edge,void* s,void* o,nd::RValue* r,int n,nd::RValue** a){
 if(pause_stats_rendering){ndship::OwnerScope canonical(1);return original(s,o,r,n,a);}
 // A visible tray is passive. Only an explicitly held keyboard shop focus,
 // or the particular click/confirmation consumed by a purchase, owns input.
 ndship::input_sharing::KeyStorage tray_keys;
 if(n>0&&n<=(int)tray_keys.size()&&a){
  int kept=0;for(int i=0;i<n;++i){double key=0;
   if(ndship::input::Numeric(a[i],key)&&key>=1&&key<=255&&std::trunc(key)==key&&Trays::BlocksKey(ndship::active_owner,(int)key))continue;
   tray_keys[kept++]=a[i];
  }
  if(!kept){*(RValue*)r=RValue(false);return r;}n=kept;a=tray_keys.data();
 }
 if(ndship::active_owner==1&&P1InputActive()){
  bool escape=false;for(int i=0;i<n;++i){double key=0;if(ndship::input::Numeric(a[i],key)&&key==VK_ESCAPE)escape=true;}
  if(!escape){SampleKeyboard(1);*(RValue*)r=RValue(p1_keyboard.EvaluateButtonHelper(n,a,edge));return r;}
 }
 if(ndship::active_owner!=2){
  if(ready&&ndship::active_owner==1&&!ndship::upgrade::SessionActive()&&!command.mutate_sharing){
   ndship::input_sharing::KeyStorage storage;auto keys=ndship::input_sharing::FilterNativeKeys(controls.Get(),n,a,storage);
   if(keys.Changed()){if(!keys.argc){*(RValue*)r=RValue(false);return r;}return original(s,o,r,keys.argc,keys.argv);}
  }
  return original(s,o,r,n,a);
 }
 if(!ndship::upgrade::SessionActive())for(int i=0;i<n;++i){double key=0;if(ndship::input::Numeric(a[i],key)&&key==VK_ESCAPE)return original(s,o,r,n,a);}
 SampleKeyboard();bool value=keyboard.EvaluateButtonHelper(n,a,edge);
 if(command.qa_focus&&ndship::upgrade::SessionActive()&&edge==ndship::input::Edge::Pressed&&value){char t[160];std::snprintf(t,sizeof t,"P2 native menu key pressed frame=%llu key=%.0f",input_frame.load(),n?a[0]->real:0);Log(t);}
 *(RValue*)r=RValue(value);return r;
}
nd::RValue* Held(void*s,void*o,nd::RValue*r,int n,nd::RValue**a){return Input(original_held,ndship::input::Edge::Held,s,o,r,n,a);}
nd::RValue* Pressed(void*s,void*o,nd::RValue*r,int n,nd::RValue**a){return Input(original_pressed,ndship::input::Edge::Pressed,s,o,r,n,a);}
nd::RValue* Released(void*s,void*o,nd::RValue*r,int n,nd::RValue**a){return Input(original_released,ndship::input::Edge::Released,s,o,r,n,a);}
nd::RValue* PadHeld(void*s,void*o,nd::RValue*r,int n,nd::RValue**a){if(ndship::active_owner==2||(ndship::active_owner==1&&P1InputActive())){*(RValue*)r=RValue(false);return r;}return original_pad_held(s,o,r,n,a);}
nd::RValue* PadPressed(void*s,void*o,nd::RValue*r,int n,nd::RValue**a){if(ndship::active_owner==2||(ndship::active_owner==1&&P1InputActive())){*(RValue*)r=RValue(false);return r;}return original_pad_pressed(s,o,r,n,a);}
nd::RValue* EndScore(void*s,void*o,nd::RValue*r,int n,nd::RValue**a){
 int owner=ndship::active_owner;int partner=owner==2?1:2;
 if(owner>=1&&owner<=2&&Asset(s)==603&&Id(s)==ships[owner]&&Resolve(ships[partner])){Log("co-op ship death; surviving partner keeps shared world running");*(RValue*)r=RValue(0.0);return r;}return original_end_score(s,o,r,n,a);
}
nd::Script original_instance_create=nullptr;
bool IsProjectile(int asset){switch(asset){case 269:case 389:case 490:case 517:case 612:case 632:case 640:case 649:case 704:case 764:case 794:case 798:case 803:return true;default:return false;}}
nd::RValue* InstanceCreate(void*s,void*o,nd::RValue*r,int n,nd::RValue**a){
 auto caller=(std::uintptr_t)_ReturnAddress()-access.base;
 int owner=ndship::active_owner;auto result=original_instance_create(s,o,r,n,a);
 if(owner>0&&result){int id=(result->kind&0xffffff)==15?(int)result->integer:-1;auto p=Resolve(id);if(p&&ndship::assets::IsTaggable(Asset(p))){Tag(p,owner);if(IsProjectile(Asset(p)))++shots[owner];
  if(Asset(p)==310&&(caller==0xc86389||caller==0xc86a15||caller==0xc836b1||caller==0xc842e8)){
   // This native effect has no Cleanup callback. Keep only runner-live IDs.
   for(auto it=health_effects.begin();it!=health_effects.end();)if(!Resolve(it->first))it=health_effects.erase(it);else ++it;
   health_effects[id]={owner,std::max(1.0,Read(Resolve(controller_id),0x1af4ea8,1))};
  }
 }}
 return result;
}
nd::Event original_player_projectile_create=nullptr,original_blaster_create=nullptr;
void ProjectileCreate(void*s,void*o){int owner=ndship::active_owner;if(!owner)owner=Owner(o);if(owner){Tag(s,owner);}original_player_projectile_create(s,o);}
void BlasterCreate(void*s,void*o){int owner=ndship::active_owner;if(!owner)owner=Owner(o);Tag(s,owner);original_blaster_create(s,o);}
nd::Script original_save_run=nullptr;
nd::RValue* SaveRun(void*s,void*o,nd::RValue*r,int n,nd::RValue**a){if(ready){*(RValue*)r=RValue(false);return r;}return original_save_run(s,o,r,n,a);}
void Stimuli(){
 if(!command.qa_focus)return;
 if(CoopBalance::qa_probe_running)return;
 if(command.rescue_hold>=1&&command.rescue_hold<=2){const int down=command.rescue_hold;auto& r=CoopFlow::rescues[down];auto partner=Resolve(ships[3-down]);if(r.down&&partner){access.set(partner,nd::Fields::X,r.x);access.set(partner,nd::Fields::Y,r.y);access.set(partner,nd::Fields::HSpeed,0);access.set(partner,nd::Fields::VSpeed,0);
  // The stationary proximity fixture must not lose its survivor to ambient
  // combat. Real collision probes run with rescue_hold cleared, without this.
  auto build=Resolve(powers[3-down]);if(build)access.set(build,nd::Fields::Hull,Read(build,0x1aeef08,100));}}
 if(command.collision){int owner=command.collision;command.collision=0;CoopFlow::ProbeCollision(owner,command.counter);return;}
 if(command.balance){int mode=command.balance;command.balance=0;CoopBalance::qa_probe_running=true;CoopBalance::Probe(mode,command.counter);CoopBalance::qa_probe_running=false;return;}
 if(command.restart){command.restart=0;if(Builtin("instance_number",{RValue(573)}).ToInt32()==0){Log("QA paused native restart object create");Builtin("instance_create_depth",{RValue(0.0),RValue(0.0),RValue(0.0),RValue(504)});}else Log("QA restart refused: enter native pause first");return;}
 if(command.upgrade){int owner=command.upgrade;command.upgrade=0;auto c=Resolve(controller_id);if(c){ndship::OwnerScope scope(owner);Script(0x357de0,c);}}
 if(command.deferred_upgrade){int owner=command.deferred_upgrade;command.deferred_upgrade=0;auto c=Resolve(controller_id);if(c){ndship::OwnerScope scope(owner);Script(0x358560,c);}}
 if(command.shield_break){int owner=command.shield_break;command.shield_break=0;if(auto b=Resolve(powers[owner])){access.set(b,0x1af7fd8,0);Builtin("variable_instance_set",{Ref(powers[owner]),RValue("shieldbreak"),RValue(0)});Builtin("variable_instance_set",{Ref(powers[owner]),RValue("regenerate"),RValue(0)});}}
 if(command.damage){int owner=command.owner;auto b=Resolve(powers[owner]);if(b){access.set(b,nd::Fields::Hull,std::max(1.0,Read(b,nd::Fields::Hull,100)-command.damage));access.set(b,0x1af7fd8,std::max(0.0,Read(b,0x1af7fd8,0)-command.damage));}command.damage=0;}
 if(command.beacon_edge>=1&&command.beacon_edge<=2){int owner=command.beacon_edge;command.beacon_edge=0;if(auto ship=Resolve(ships[owner])){access.set(ship,nd::Fields::X,Global(0x1af7428)+Global(0x1af7468)*(owner==2?.03:.97));access.set(ship,nd::Fields::Y,Global(0x1af74a8)+Global(0x1af7418)*.5);access.set(ship,nd::Fields::HSpeed,0);access.set(ship,nd::Fields::VSpeed,0);}}
 if(command.beacon_center>=1&&command.beacon_center<=2){int owner=command.beacon_center;command.beacon_center=0;for(int pilot=1;pilot<=2;++pilot)if(auto ship=Resolve(ships[pilot])){access.set(ship,nd::Fields::X,Global(0x1af7428)+Global(0x1af7468)*(pilot==owner?.45:.65));access.set(ship,nd::Fields::Y,Global(0x1af74a8)+Global(0x1af7418)*.42);access.set(ship,nd::Fields::HSpeed,0);access.set(ship,nd::Fields::VSpeed,0);}}
 if(command.kill){int owner=command.kill;command.kill=0;auto b=Resolve(powers[owner]);if(b){access.set(b,0x1af7f98,0);access.set(b,nd::Fields::Hull,0);}}
 if(command.tray_enemy){command.tray_enemy=0;ndship::OwnerScope world(0);auto enemy=Builtin("instance_create_depth",{RValue(Global(0x1af7428)+100),RValue(Global(0x1af74a8)+100),RValue(0.0),RValue(252)});auto e=Resolve((int)enemy.m_i64);if(e){access.set(e,nd::Fields::HSpeed,1);access.set(e,nd::Fields::VSpeed,.5);}}
 if(command.native_augment>0){int asset=command.native_augment,owner=command.owner;command.native_augment=0;ndship::OwnerScope scope(owner);auto ship=Resolve(ships[owner]);nd::RValue args[4]{};args[0].real=Read(ship,nd::Fields::X,0);args[1].real=Read(ship,nd::Fields::Y,0);args[2].kind=15;args[2].integer=0x0100000000000000ULL|(unsigned)asset;args[3].kind=5;nd::RValue* argv[]={args,args+1,args+2,args+3};nd::RValue out{};out.kind=5;((nd::Script)(access.base+0x3a2d60))(ship,ship,&out,4,argv);}
 if(command.augment>0){int asset=command.augment,owner=command.owner;command.augment=0;ndship::OwnerScope scope(owner);auto created=Builtin("instance_create_depth",{RValue(0.0),RValue(0.0),RValue(0.0),RValue(asset)},Resolve(powers[owner]));Tag(Resolve((int)created.m_i64),owner);char t[160];std::snprintf(t,sizeof t,"QA native augment Create asset=%d owner=%d; awaiting engine Alarm",asset,owner);Log(t);}
 if(command.xp){command.xp=0;auto c=Resolve(controller_id);auto player=Resolve(ships[1]);if(!player)player=Resolve(ships[2]);if(c&&player){ndship::OwnerScope owner(1);double factor=Global(0x1affeb8)*Global(0x1aee5c8)*CoopBalance::XPMultiplier();if(factor>0){nd::RValue args[4]{};args[0].real=(Read(c,0x1afd8e8)-Read(c,0x1a9da98)+1)/factor;args[1].real=0;args[2].kind=args[3].kind=13;nd::RValue* argv[]={&args[0],&args[1],&args[2],&args[3]};nd::RValue r{};r.kind=5;CoopBalance::qa_xp_award=true;((nd::Script)(access.base+0x781240))(player,player,&r,4,argv);CoopBalance::qa_xp_award=false;Log("QA native pickup-equivalent XP; awaiting shared Controller expControl");}}}
 if(command.level>0){int count=command.level;command.level=0;auto c=Resolve(controller_id);if(c){nd::RValue r{};r.kind=5;nd::RValue arg{};arg.real=count;nd::RValue* args[]={&arg};++CoopBalance::team_level_scope;((nd::Script)(access.base+0x2fc6c0))(c,c,&r,1,args);--CoopBalance::team_level_scope;}}
 if(command.screen){command.screen=0;save_native_ui=true;Builtin("screen_save",{RValue("fullship-screen.png")});}
}
nd::RValue* GainLevel(void*s,void*o,nd::RValue*r,int n,nd::RValue**a){
 // Trait-granted levels (for example Grandeur) belong to their build. Only
 // Controller XP progression grants the partner a matching upgrade choice.
 if(ready&&!CoopBalance::team_level_scope){
  const int owner=ndship::active_owner;
  if(owner<1||owner>2)return original_gain_level(s,o,r,n,a);
  level_award_inflight=true;
  const double team_level=Global(0x1af9148),added=Global(0x1af8bc8);
  auto out=original_gain_level(s,o,r,n,a);const double delta=Global(0x1af9148)-team_level;
  SetGlobal(0x1af9148,team_level);SetGlobal(0x1af8bc8,added+delta);SetGlobal(0x1af98c8,team_level+added+delta);
  level_award_inflight=false;Log("Trait bonus retained by its build; shared XP level preserved");return out;
 }
 level_award_inflight=true;int primary=ready&&!HasShip(1)&&HasShip(2)?2:1;int second=primary==1?2:1;
 ndship::OwnerScope first(primary);double before=globals?Global(0x1af8ce8):0;auto out=original_gain_level(s,o,r,n,a);double after=globals?Global(0x1af8ce8):0;
 if(ready&&after>before){ndship::OwnerScope partner(second);SetGlobal(0x1af8ce8,Global(0x1af8ce8)+after-before);SetGlobal(0x1af98c8,Global(0x1af9148)+Global(0x1af8bc8));if(HasShip(second))Script(0x4c79f0,Resolve(powers[second]));Log("shared level award mirrored into independent partner choice budget");
  for(int owner=1;owner<=2;++owner){ndship::OwnerScope pick(owner);if(Global(0x1af8938)<4)Trays::Request(owner);}
 }
 level_award_inflight=false;return out;
}
#include "ship_balance_probe.hpp"
bool Hook(const char* name,std::uintptr_t rva,void* detour,void** original){auto status=MmCreateHook(module_self,name,(void*)(access.base+rva),detour,original);char t[180];std::snprintf(t,sizeof t,"hook %s RVA=%llx status=%s",name,rva,AurieStatusToString(status));Log(t);return AurieSuccess(status);}
bool HookBuiltin(const char* name,TRoutine detour,TRoutine& original){void* target=nullptr;auto status=api->GetNamedRoutinePointer(name,&target);return AurieSuccess(status)&&AurieSuccess(MmCreateHook(module_self,name,target,(void*)detour,(void**)&original));}
// BurstThrust is a shared effect class, but a pilot-tagged instance follows
// that pilot directly. Retire it without making neutral effects owner-filtered.
std::vector<int> OwnedIds(int owner){std::vector<int> ids;for(auto [id,which]:owners)if(which==owner){auto p=ndship::lifecycle::ResolveExisting(id);int asset=Asset(p);if(ndship::assets::IsOwned(asset)||asset==828||asset==572||asset==674||(asset==317&&!(qa_mode&&command.mutate_thrust_cleanup)))ids.push_back(id);}return ids;}
void Forget(int id,void* ptr){owners.erase(id);if(ptr)ndship::UnregisterOwnedInstance(ptr);}
void DisposeList(int handle){Builtin("ds_list_destroy",{RValue(handle)});}
void BeginTeardown(ndship::lifecycle::Reason reason){joining=true;command.join=command.augment=command.level=command.xp=command.upgrade=0;}
void EndTeardown(ndship::lifecycle::Reason reason,const ndship::lifecycle::Report& report){
 if(reason!=ndship::lifecycle::Reason::ShipDeath){CoopFlow::Reset();Trays::Reset();}
 if(reason!=ndship::lifecycle::Reason::ShipDeath){CoopBalance::targets.clear();CoopBalance::spawn_carry=0;}
 if(reason!=ndship::lifecycle::Reason::ShipDeath){ResetNativeUI();ready=false;ships[2]=powers[2]=-1;ndship::ClearBindings();if(Resolve(ships[1])){ndship::Bind(0,603,ships[1]);ndship::Bind(1,603,ships[1]);}if(Resolve(powers[1])){ndship::Bind(0,621,powers[1]);ndship::Bind(1,621,powers[1]);}ndship::upgrade::Reset();}
 if(reason==ndship::lifecycle::Reason::ShipDeath){int survivor=ndship::active_owner==2?1:2;if(Resolve(ships[survivor]))ndship::Bind(0,603,ships[survivor]);}
 joining=false;
}
bool HasShip(int owner){auto p=ndship::lifecycle::ResolveExisting(ships[owner]);return p&&!(ndship::lifecycle::FlagsOf(p)&0x100001);}
double Budget(int owner){if(owner==2&&!ready)return 0;if(ready&&!HasShip(owner))return 0;if(!globals||!original_global_read)return 0;ndship::OwnerScope scope(owner);return Global(0x1af8ce8);}
void SetBudget(int owner,double value){ndship::OwnerScope scope(owner);SetGlobal(0x1af8ce8,value);}
void* Controller(){return Resolve(controller_id);}
int CountUI(){return Builtin("instance_number",{RValue(828)}).ToInt32();}
#include "ship_trays_runtime.hpp"
#include "ship_pause_runtime.hpp"
// The game's status renderer caches health and shield animation on Controller.
// Keep its P2 cache separate while letting native geometry and draw code run.
constexpr std::uintptr_t UIFields[]={
 0x1b09978, // barWidth
 0x1b09958, // barX
 0x1af4ec8, // barX1
 0x1b09928, // barX2
 0x1af4ea8, // barShieldWidth
 0x1af4eb8, // barShieldX
 0x1af4e88, // barShieldX1
 0x1af4e98, // barShieldX2
 0x1aedb48, // currentHull
 0x1aedb38, // currentShield
 0x1aedb58, // targHull
 0x1aedba8, // targShield
 0x1aedb08, // colHull
 0x1aedc08, // colShield
 0x1aedbb8, // colHullBar
 0x1b04158, // colHullBack
 0x1b09a98, // colHullReserved
 0x1aedb68, // colFlashBar
 0x1aedbf8, // colShieldBar
 0x1af6668, // colShieldBack
 0x1af66a8, // colShieldReserved
 0x1aedc18, // enemyDamageHullFlashOpacity
 0x1aedbd8, // enemyDamageShieldFlashOpacity
 0x1aedb88, // selfDamageHullFlashOpacity
 0x1aedbe8, // selfDamageShieldFlashOpacity
 0x1b09a38, // timeAlpha
 0x1aeebf8, // wrapDisabledAlpha
};
using UIValues=std::array<RValue,std::size(UIFields)>;
UIValues p2_ui_values;
UIValues ReadUI(void* controller){UIValues values;for(size_t i=0;i<values.size();++i)values[i]=Field(controller,UIFields[i]);return values;}
void WriteUI(void* controller,const UIValues& values,size_t count=std::size(UIFields)){
 for(size_t i=0;i<count;++i)((nd::SetValue)(access.base+0xf17460))(controller,access.id(UIFields[i]),nd::Scalar,(nd::RValue*)&values[i]);
}
void InitializeP2UI(void* controller){
 if(p2_ui_valid)return;p2_ui_values=ReadUI(controller);
 auto b=Resolve(powers[2]);p2_ui_values[8]=p2_ui_values[10]=RValue(Read(b,0x1aedbc8,1));p2_ui_values[9]=p2_ui_values[11]=RValue(Read(b,0x1aedb98,1));
 for(size_t i=21;i<25;++i)p2_ui_values[i]=RValue(0.0);p2_ui_valid=true;
}
nd::Script original_ui_flash=nullptr,original_ui_positions=nullptr,original_ui_refresh=nullptr;
nd::RValue* UIScript(nd::Script original,void*s,void*o,nd::RValue*r,int n,nd::RValue**a){
 auto controller=Resolve(controller_id);
 if(ndship::active_owner!=2||ui_overlay_active||!controller||powers[2]<100000||!HasShip(1)||ndship::lifecycle::IsTearingDown())return original(s,o,r,n,a);
 auto before=ReadUI(controller);InitializeP2UI(controller);WriteUI(controller,p2_ui_values,25);ui_overlay_active=true;
 auto result=original(s,o,r,n,a);p2_ui_values=ReadUI(controller);WriteUI(controller,before);ui_overlay_active=false;return result;
}
nd::RValue* UIFlash(void*s,void*o,nd::RValue*r,int n,nd::RValue**a){return UIScript(original_ui_flash,s,o,r,n,a);}
nd::RValue* UIPositions(void*s,void*o,nd::RValue*r,int n,nd::RValue**a){return UIScript(original_ui_positions,s,o,r,n,a);}
nd::RValue* UIRefresh(void*s,void*o,nd::RValue*r,int n,nd::RValue**a){return UIScript(original_ui_refresh,s,o,r,n,a);}
ndship::surface_hud::NativeSurfaceRenderer native_hud;
ndship::surface_hud::Result native_hud_result;
std::array<ndship::surface_hud::Rect,3> native_life_region{},native_stock_region{};
double native_shield_center[3]{},native_shield_width[3]{};
nd::Event original_health_effect_draw=nullptr;
unsigned long long health_effect_draws[3]{};
void HealthEffectDraw(void*s,void*o){
 auto found=health_effects.find(Id(s));
 if(!ready||command.nativehud||!native_hud_result.ok||found==health_effects.end()){original_health_effect_draw(s,o);return;}
 const auto effect=found->second;const int owner=effect.owner;
 const auto source=native_hud_result.source[owner],dest=native_hud_result.dest[owner];
 if(source.w<=0||source.h<=0){original_health_effect_draw(s,o);return;}
 // These native GUI bursts are independent objects, not part of Controller's
 // surface. Move only the four health/shield callsites; world bursts stay native.
 RValue x,y,xscale,yscale;
 api->GetBuiltin("x",(CInstance*)s,NULL_INDEX,x);api->GetBuiltin("y",(CInstance*)s,NULL_INDEX,y);
 api->GetBuiltin("image_xscale",(CInstance*)s,NULL_INDEX,xscale);api->GetBuiltin("image_yscale",(CInstance*)s,NULL_INDEX,yscale);
 double sx=dest.w/source.w,sy=dest.h/source.h;
 RValue nx(dest.x+(native_shield_center[owner]-source.x)*sx),ny(dest.y+(y.ToDouble()-source.y)*sy);
 RValue nsx(xscale.ToDouble()*sx*native_shield_width[owner]/effect.width),nsy(yscale.ToDouble()*sy);
 api->SetBuiltin("x",(CInstance*)s,NULL_INDEX,nx);api->SetBuiltin("y",(CInstance*)s,NULL_INDEX,ny);
 api->SetBuiltin("image_xscale",(CInstance*)s,NULL_INDEX,nsx);api->SetBuiltin("image_yscale",(CInstance*)s,NULL_INDEX,nsy);
 if(qa_mode&&command.mutate_healthfx){api->SetBuiltin("x",(CInstance*)s,NULL_INDEX,x);api->SetBuiltin("y",(CInstance*)s,NULL_INDEX,y);}
 RValue drawn_x,drawn_y;api->GetBuiltin("x",(CInstance*)s,NULL_INDEX,drawn_x);api->GetBuiltin("y",(CInstance*)s,NULL_INDEX,drawn_y);
 original_health_effect_draw(s,o);++health_effect_draws[owner];
 if(CoopBalance::trace){std::fprintf(CoopBalance::trace,"{\"kind\":\"health_effect\",\"counter\":%d,\"owner\":%d,\"x\":%.3f,\"y\":%.3f,\"width\":%.3f,\"source_x\":%.3f,\"source_y\":%.3f}\n",command.counter,owner,drawn_x.ToDouble(),drawn_y.ToDouble(),Builtin("display_get_gui_width").ToDouble(),x.ToDouble(),y.ToDouble());std::fflush(CoopBalance::trace);}
 api->SetBuiltin("x",(CInstance*)s,NULL_INDEX,x);api->SetBuiltin("y",(CInstance*)s,NULL_INDEX,y);
 api->SetBuiltin("image_xscale",(CInstance*)s,NULL_INDEX,xscale);api->SetBuiltin("image_yscale",(CInstance*)s,NULL_INDEX,yscale);
}
nd::Event original_stock_effect_draw=nullptr;
void StockEffectDraw(void*s,void*o){if(!StockEffectCaptured())original_stock_effect_draw(s,o);}
int native_capture_owner=0;
int native_stock_effects[3]{};
bool StockEffectCaptured(){return ready&&!command.nativehud&&native_hud_result.ok;}
unsigned long long native_hud_frames=0;
struct NativeBarState{double hull_back=0,hull_fill=0,shield_back=0,shield_fill=0;};
NativeBarState native_bars[3];

using DrawRectangle=void(*)(float,float,float,float,bool);
DrawRectangle original_draw_rectangle=nullptr;
void HealthRectangle(float x1,float y1,float x2,float y2,bool outline){
 auto caller=(std::uintptr_t)_ReturnAddress()-access.base;
 if(native_capture_owner){auto& bar=native_bars[native_capture_owner];switch(caller){
  case 0x98a93b:bar.hull_back=x2-x1;break;case 0x98abcb:bar.hull_fill=x2-x1;break;
  case 0x98af23:bar.shield_back=x2-x1;break;case 0x98b1b3:bar.shield_fill=x2-x1;break;
 }}
 original_draw_rectangle(x1,y1,x2,y2,outline);
}

nd::Event original_draw_gui=nullptr,original_upgrade_prompt=nullptr;
bool CompactPresentation(){return ready&&(Trays::CombatStage(1)||Trays::CombatStage(2));}
void UpgradePrompt(void*s,void*o){if(!CompactPresentation())original_upgrade_prompt(s,o);}
nd::Event original_upgrade_tutorial=nullptr,original_generic_tutorial=nullptr,original_generic_tutorial2=nullptr;
void RememberTutorialEffects(void* self){
 // TutorialUpgrade.effect is a child instance. Its graphic/pulseGraphic
 // fields and the generic tutorial's graphic fields are sprite references.
 auto value=Field(self,0x1af7c38);double numeric=0;
 int id=value.m_Kind==VALUE_REF?(int)value.m_i64:ndship::build::Number(*(nd::RValue*)&value,numeric)?(int)numeric:-1;
 if(id>=100000&&Asset(Resolve(id))==571)hidden_tutorial_effects.insert(id);
}
void UpgradeTutorial(void*s,void*o){if(CompactPresentation())RememberTutorialEffects(s);else original_upgrade_tutorial(s,o);}
bool ReplacedAdviceText(const RValue& text){
 if(!globals||text.m_Kind!=VALUE_STRING)return false;
 const std::string value=text.ToString();
 // Compare localized native messages; leave encounter warnings and other
 // generic tutorials intact. These are presentation-only DrawGUI hooks.
 for(auto cell:{0x1b04818ULL,0x1b03f18ULL,0x1b03e58ULL,0x1b03f08ULL,0x1b03ef8ULL,0x1b01bb8ULL,0x1b01bd8ULL,0x1b01be8ULL,0x1a9bb28ULL,0x1a9bb98ULL,0x1a9bbb8ULL,0x1a9bbf8ULL,0x1a9bd08ULL,0x1a9bd18ULL,0x1a9cb08ULL,0x1a9d338ULL,0x1a9d558ULL,0x1a9d7a8ULL,0x1a9d818ULL,0x1b0a9d8ULL,0x1b0aaa8ULL,0x1b0aae8ULL,0x1b0ac18ULL}){
  auto native=(RValue*)GlobalRead(globals,access.id(cell));if(native&&native->m_Kind==VALUE_STRING&&value==native->ToString())return true;
 }
 return false;
}
bool ObsoleteText(const RValue& text){return CompactPresentation()&&ReplacedAdviceText(text);}
bool ObsoleteAdvice(void* self){return CompactPresentation()&&ObsoleteText(Field(self,0x1b01048));}
void GenericTutorial(void*s,void*o){if(!ObsoleteAdvice(s))original_generic_tutorial(s,o);}
void GenericTutorial2(void*s,void*o){if(!ObsoleteAdvice(s))original_generic_tutorial2(s,o);}
nd::Script original_construct_tutorial=nullptr;
nd::RValue* ConstructTutorial(void*s,void*o,nd::RValue*r,int n,nd::RValue**a){
 // The SP tutorial's full-screen key legend uses one input profile. Personal
 // stock strips and the configured co-op controls provide the combat cues.
 if(ready){*(RValue*)r=RValue();return r;}
 return original_construct_tutorial(s,o,r,n,a);
}
nd::Script original_spawn_tutorial=nullptr;
nd::RValue* SpawnTutorial(void*s,void*o,nd::RValue*r,int n,nd::RValue**a){
 if(n>0&&a&&a[0]&&ObsoleteText(*(RValue*)a[0])){*(RValue*)r=RValue();return r;}
 return original_spawn_tutorial(s,o,r,n,a);
}
nd::Event original_tutorial_effect=nullptr;
void TutorialEffect(void*s,void*o){
 if(CompactPresentation()){
  if(!hidden_tutorial_effects.count(Id(s))){
   int n=Builtin("instance_number",{RValue(823)}).ToInt32();
   for(int i=0;i<n;++i){auto parent=Builtin("instance_find",{RValue(823),RValue(i)});if(auto tutorial=Resolve((int)parent.m_i64))RememberTutorialEffects(tutorial);}
  }
  if(hidden_tutorial_effects.count(Id(s)))return;
 }
 original_tutorial_effect(s,o);
}
void PerformanceLog(){
 if(!performance||GetTickCount64()-performance_at<1000)return;
 performance_at=GetTickCount64();
 for(unsigned i=0;i<(unsigned)ndship::perf::Metric::Count;++i){
  auto value=ndship::perf::Read((ndship::perf::Metric)i);
  std::fprintf(performance,"{\"time\":%llu,\"metric\":%u,\"samples\":%llu,\"mean_ms\":%.6f,\"p50_ms\":%.6f,\"p95_ms\":%.6f,\"p99_ms\":%.6f,\"max_ms\":%.6f}\n",performance_at,i,value.total_samples,value.mean_ms,value.p50_ms,value.p95_ms,value.p99_ms,value.max_ms);
 }
 std::fflush(performance);
}
bool config_pending=false;
HANDLE editor_process=nullptr;DWORD editor_pid=0;
void OpenControlEditor(){
 if(editor_process&&WaitForSingleObject(editor_process,0)!=WAIT_TIMEOUT){CloseHandle(editor_process);editor_process=nullptr;if(ready)hint_until[2]=GetTickCount64()+4500;}
 if((GetAsyncKeyState(VK_F6)&1)&&Builtin("window_has_focus").ToBoolean())config_pending=true;
 if(qa_mode&&command.configure){config_pending=true;command.configure=0;}
 if(!config_pending)return;
 auto controller=Resolve(controller_id);
 if(controller&&!ndship::upgrade::SessionActive()&&(HasShip(1)||HasShip(2))&&Read(controller,0x1b004b8,0)==0){
  ndship::OwnerScope one(1);Script(0x460770,controller);return;
 }
 config_pending=false;
 if(editor_process){
  if(WaitForSingleObject(editor_process,0)==WAIT_TIMEOUT){
   EnumWindows([](HWND window,LPARAM parameter)->BOOL{DWORD pid=0;GetWindowThreadProcessId(window,&pid);if(pid==(DWORD)parameter&&IsWindowVisible(window)){ShowWindow(window,SW_RESTORE);SetForegroundWindow(window);}return TRUE;},(LPARAM)editor_pid);return;
  }
  CloseHandle(editor_process);editor_process=nullptr;
 }
 auto executable=root/L"NovaDriftP2Controls.exe";
 std::wstring arguments=L"\""+executable.wstring()+L"\"";
 STARTUPINFOW startup{};startup.cb=sizeof startup;PROCESS_INFORMATION process{};
 if(CreateProcessW(executable.c_str(),arguments.data(),nullptr,nullptr,FALSE,0,nullptr,root.c_str(),&startup,&process)){
  CloseHandle(process.hThread);editor_process=process.hProcess;editor_pid=process.dwProcessId;char text[128];std::snprintf(text,sizeof text,"P2 controls editor opened pid=%lu",editor_pid);Log(text);
 }else Log("P2 controls editor could not open; use ConfigureP2.cmd");
}
bool DrawNativeHud(void*s,void*o,int primary,double width,double height){
 auto controller=Resolve(controller_id);if(!controller)return false;
 const double scale=Global(0x1b02718);if(!std::isfinite(scale)||scale<=0)return false;
 if(primary!=ui_primary){if(primary==2&&p2_ui_valid)WriteUI(controller,p2_ui_values,25);ui_primary=primary;}
 native_hud_result=native_hud.Capture(api,(CInstance*)s,width,height,[&](int owner){
  ndship::OwnerScope scope(owner);auto before=ReadUI(controller);bool secondary=owner!=primary;
  if(owner==2&&secondary&&!command.mutate_hud){InitializeP2UI(controller);WriteUI(controller,p2_ui_values,25);}
  ui_overlay_active=true;
  Script(0x4483b0,controller);
  if(owner==2&&secondary&&!command.mutate_hud)Script(0x370b50,controller);
  if(!HasShip(owner)){access.set(controller,0x1aedb48,0);access.set(controller,0x1aedb38,0);}
  constexpr std::uintptr_t keyCells[]={0x1af3208,0x1af3388,0x1af31e8,0x1af3318,0x1af3288};
  std::array<RValue,5> savedKeys;
  for(int k=0;k<5;++k){
   auto slot=(RValue*)GlobalWrite(globals,access.id(keyCells[k]));savedKeys[k]=*slot;
   if(owner==2||p1_custom){
    const auto& profile=owner==1?p1_controls.Get():controls.Get();
    auto action=(ndship::config::Action)((int)ndship::config::Action::Construct1+k);
    int binding=profile.device==ndship::config::Device::Controller?profile.Pad(action):profile.Key(action);
    *slot=RValue((profile.device==ndship::config::Device::Controller?binding==0:binding<0)?std::string{}:Trays::Binding(action,owner));
   }
  }
  native_capture_owner=owner;native_bars[owner]={};original_draw_gui(s,o);native_capture_owner=0;
  // Construction flashes draw at the native icon coordinates. Capture them
  // with their owner's strip so they move with the icons, using native artwork.
  native_stock_effects[owner]=0;
  if(HasShip(owner)){
   int effects=Builtin("instance_number",{RValue(365)}).ToInt32();
   for(int i=0;i<effects;++i){auto ref=Builtin("instance_find",{RValue(365),RValue(i)});auto effect=Resolve((int)ref.m_i64);
    if(effect&&Owner(effect)==owner){original_stock_effect_draw(effect,effect);++native_stock_effects[owner];}
   }
  }
  for(int k=0;k<5;++k)*(RValue*)GlobalWrite(globals,access.id(keyCells[k]))=savedKeys[k];
  double left=std::min(Read(controller,0x1b09958,0),Read(controller,0x1af4eb8,0))-3*scale;
  double right=std::max(Read(controller,0x1b09928,width),Read(controller,0x1af4e98,width))+3*scale;
  native_life_region[owner]={left,height-38*scale,std::max(1.0,right-left),26*scale};
  native_shield_width[owner]=Read(controller,0x1af4ea8,0);
  native_shield_center[owner]=Read(controller,0x1af4eb8,0)+native_shield_width[owner]/2;
  // Native Controller ammo/construct geometry is shared, but every reserve,
  // selected slot and assembly meter above was drawn under this owner's scope.
  double stock_left=width,stock_right=0;
  for(auto cell:{0x1af4d18u,0x1af4c78u,0x1af4bf8u,0x1af4cf8u,0x1af4ca8u,0x1af4c28u,0x1af4c58u}){
   double x=Read(controller,cell,0);stock_left=std::min(stock_left,x);stock_right=std::max(stock_right,x+110*scale);
  }
  double stock_top=std::min(Read(controller,0x1af4d78,0),Read(controller,0x1af4d38,0))-12*scale;
  double stock_bottom=std::max(Read(controller,0x1af4c88,0),Read(controller,0x1af4d68,0))+32*scale;
  native_stock_region[owner]={stock_left-12*scale,stock_top,stock_right-stock_left+24*scale,stock_bottom-stock_top};
  if(owner==2)p2_ui_values=ReadUI(controller);
  if(secondary)WriteUI(controller,before);
  ui_overlay_active=false;
 },[&](int owner){return native_life_region[owner];},[&](int owner){return native_stock_region[owner];},primary,scale,qa_mode&&command.mutate_stock);
 if(native_hud_result.ok){++native_hud_frames;native_rect_hidden+=6;
  if(CoopBalance::trace){static ULONGLONG stock_at=0;auto now=GetTickCount64();if(now-stock_at>=100){stock_at=now;
   for(int owner=1;owner<=2;++owner){auto b=Resolve(powers[owner]);auto src=native_hud_result.stock_source[owner],dst=native_hud_result.stock_dest[owner];
    std::fprintf(CoopBalance::trace,"{\"kind\":\"stock\",\"command\":%d,\"owner\":%d,\"sx\":%.3f,\"sy\":%.3f,\"sw\":%.3f,\"sh\":%.3f,\"dx\":%.3f,\"dy\":%.3f,\"dw\":%.3f,\"dh\":%.3f,\"blade\":%.4f,\"swords\":%.4f,\"mines\":%.4f,\"turrets\":%.4f,\"drones\":%.4f,\"allies\":%.4f,\"ally_progress\":%.4f,\"effects\":%d}\n",command.counter,owner,src.x,src.y,src.w,src.h,dst.x,dst.y,dst.w,dst.h,Read(b,0x1afd438,0),Read(b,0x1afd498,0),Read(b,0x1b019b8,0),Read(b,0x1afd398,0),Read(b,0x1afd368,0),Read(b,0x1afd2e8,0),Read(b,0x1aef1b8,0),native_stock_effects[owner]);
   }std::fflush(CoopBalance::trace);
  }}
  if(save_native_ui){native_hud.Save(1,"native-hud-1.png");native_hud.Save(2,"native-hud-2.png");save_native_ui=false;}
 }
 return native_hud_result.ok;
}
// World-space indicators use GUI projection, but stay below every HUD layer.
void DrawWorldOverlays(void* self,double width,double height){
 if(!ready)return;
 double view_x=Global(0x1af7428),view_y=Global(0x1af74a8),view_w=Global(0x1af7468),view_h=Global(0x1af7418);
 bool show_markers=!ndship::upgrade::SessionActive()&&Read(Resolve(controller_id),0x1b004b8,0)==0;
 if(show_markers&&view_w>0&&view_h>0)for(int owner=1;owner<=2;++owner)if(auto ship=Resolve(ships[owner])){
  double x=(Read(ship,nd::Fields::X)-view_x)*width/view_w,y=(Read(ship,nd::Fields::Y)-view_y)*height/view_h;
  ndship::hud::TintShipVisual(api,(CInstance*)ship,owner);
  ndship::hud::DrawMarker(api,owner,x,y,36*width/view_w,(CInstance*)self,Budget(owner)>0?0:HintOpacity(hint_until[owner],GetTickCount64()));++marker_draws[owner];
 }
}
void DrawGUI(void*s,void*o){
 ndship::perf::MarkFrame();ReloadControls();OpenControlEditor();
 int primary=ready&&!HasShip(1)&&HasShip(2)?2:1;
 ndship::OwnerScope canonical(primary);
 // With no surviving ship, let the native end screen draw its controls and
 // fades once. The combat HUD capture hides native control prompts.
 if(ready&&!HasShip(1)&&!HasShip(2)&&Read(Resolve(controller_id),0x1af7718,0)!=0){
  original_draw_gui(s,o);input_frame.fetch_add(1,std::memory_order_relaxed);return;
 }
 double old_controls=0;bool hide_controls=ready&&globals&&original_global_read;
 if(hide_controls){old_controls=Global(0x1b002c8);SetGlobal(0x1b002c8,0);}
 double gui_width=Builtin("display_get_gui_width").ToDouble(),gui_height=Builtin("display_get_gui_height").ToDouble();
 DrawWorldOverlays(s,gui_width,gui_height);
 bool native_ok=ready&&!command.nativehud&&DrawNativeHud(s,o,primary,gui_width,gui_height);
 if(!native_ok)original_draw_gui(s,o);
 if(hide_controls)SetGlobal(0x1b002c8,old_controls);
 input_frame.fetch_add(1,std::memory_order_relaxed);
 if(!ready)return;
 ndship::perf::Span measure(ndship::perf::Metric::Hud);
 ndship::hud::State state;state.enabled=!command.nativehud;
 state.width=gui_width;state.height=gui_height;
 state.font=Builtin("draw_get_font").ToInt32();
 if(native_ok)for(int i=0;i<2;++i){auto rect=native_hud_result.dest[i+1];state.life_x[i]=rect.x;state.life_y[i]=rect.y;state.life_width[i]=rect.w;state.stock_y[i]=native_hud_result.stock_dest[i+1].y;}
 auto now=GetTickCount64();for(int i=0;i<2;++i)state.hint_alpha[i]=Budget(i+1)>0?0:HintOpacity(hint_until[i+1],now);state.controls_alpha=Budget(2)>0?0:state.hint_alpha[1];

 state.upgrade_menu=ndship::upgrade::SessionActive()&&CountUI()>0;state.upgrade_owner=ndship::upgrade::CurrentMenuOwner();
 if(state.upgrade_menu&&(!upgrade_was_open||upgrade_hint_owner!=state.upgrade_owner))upgrade_hint_until=now+2500;
 upgrade_was_open=state.upgrade_menu;upgrade_hint_owner=state.upgrade_owner;state.upgrade_hint_alpha=HintOpacity(upgrade_hint_until,now);
 for(int owner=1;owner<=2;++owner){auto b=Resolve(powers[owner]);auto& player=state.players[owner-1];
  player.joined=b!=nullptr;player.alive=HasShip(owner);player.hull=Read(b,nd::Fields::Hull,0);
  player.hull_max_effective=Read(b,0x1aeef08,0);player.shield=Read(b,0x1af7fd8,0);player.shield_max_effective=Read(b,0x1aee8a8,0);player.choices=Budget(owner);
 }
 state.menu_hint=state.upgrade_owner==2?menu_help:"P1: point at an upgrade and click to select";
 state.gameplay_hint=gameplay_help;
 ndship::hud::Draw(api,state,(CInstance*)s);
 Trays::Draw(s,gui_width,gui_height);PauseBuild::Draw(s,gui_width,gui_height);++hud_frames;
 if(presentation){static ULONGLONG at=0;auto now=GetTickCount64();if(now-at>=100){at=now;
  RValue color1(-1),color2(-1);if(auto ship=Resolve(ships[1]))api->GetBuiltin("image_blend",(CInstance*)ship,NULL_INDEX,color1);if(auto ship=Resolve(ships[2]))api->GetBuiltin("image_blend",(CInstance*)ship,NULL_INDEX,color2);
  std::fprintf(presentation,"{\"time\":%llu,\"command\":%d,\"hull1\":%.6f,\"hullmax1\":%.6f,\"shield1\":%.6f,\"shieldmax1\":%.6f,\"hull2\":%.6f,\"hullmax2\":%.6f,\"shield2\":%.6f,\"shieldmax2\":%.6f,\"color1\":%d,\"color2\":%d,\"marker1\":%llu,\"marker2\":%llu,\"native_rect_hidden\":%llu,\"hud_frames\":%llu,\"menu_owner\":%d,\"menu\":%d,\"fire_key\":%d,\"steering\":%d,\"key\":%d,\"engine_key\":%d,\"engine_input\":%d,\"p2_fire\":%d,\"p1_thrust\":%d,\"hint1\":%.6f,\"hint2\":%.6f,\"upgrade_hint\":%.6f,\"native_hud\":%d,\"native_frames\":%llu,\"bar1\":%.6f,\"bar2\":%.6f,\"src1_x\":%.6f,\"src1_y\":%.6f,\"src1_w\":%.6f,\"src1_h\":%.6f,\"src2_x\":%.6f,\"src2_y\":%.6f,\"src2_w\":%.6f,\"src2_h\":%.6f,\"dst1_x\":%.6f,\"dst1_y\":%.6f,\"dst1_w\":%.6f,\"dst1_h\":%.6f,\"dst2_x\":%.6f,\"dst2_y\":%.6f,\"dst2_w\":%.6f,\"dst2_h\":%.6f}\n",
   now,command.counter,state.players[0].hull,state.players[0].hull_max_effective,state.players[0].shield,state.players[0].shield_max_effective,state.players[1].hull,state.players[1].hull_max_effective,state.players[1].shield,state.players[1].shield_max_effective,color1.ToInt32(),color2.ToInt32(),marker_draws[1],marker_draws[2],native_rect_hidden,hud_frames,state.upgrade_owner,state.upgrade_menu,controls.Get().Key(ndship::config::Action::Fire),(int)controls.Get().steering,command.key,command.engine_key,command.engine_input,keyboard.Check(ndship::input::KeyFire,ndship::input::Edge::Held),p1_keyboard.Check(ndship::input::KeyThrust,ndship::input::Edge::Held),state.hint_alpha[0],state.hint_alpha[1],state.upgrade_hint_alpha,native_ok,native_hud_frames,
   native_bars[1].hull_back>0?native_bars[1].hull_fill/native_bars[1].hull_back:0,native_bars[2].hull_back>0?native_bars[2].hull_fill/native_bars[2].hull_back:0,
   native_hud_result.source[1].x,native_hud_result.source[1].y,native_hud_result.source[1].w,native_hud_result.source[1].h,
   native_hud_result.source[2].x,native_hud_result.source[2].y,native_hud_result.source[2].w,native_hud_result.source[2].h,
   native_hud_result.dest[1].x,native_hud_result.dest[1].y,native_hud_result.dest[1].w,native_hud_result.dest[1].h,
   native_hud_result.dest[2].x,native_hud_result.dest[2].y,native_hud_result.dest[2].w,native_hud_result.dest[2].h);std::fflush(presentation);
 }}
 PerformanceLog();

}
}
EXPORTED AurieStatus ModuleInitialize(AurieModule* module,const fs::path& path){
 module_self=module;root=path.parent_path().parent_path().parent_path();log_file=_wfsopen((root/L"ship-events.log").c_str(),L"a",_SH_DENYNO);CommandsRead(true);qa_mode=command.qa_focus!=0;diagnostics=command.diagnostics!=0;if(qa_mode||diagnostics)telemetry=_wfsopen((root/L"ship-telemetry.jsonl").c_str(),L"a",_SH_DENYNO);Log("full native ship experimental initialize");
 ndship::perf::SetEnabled(qa_mode||diagnostics);if(qa_mode||diagnostics)performance=_wfsopen((root/L"ship-performance.jsonl").c_str(),L"a",_SH_DENYNO);
 if(qa_mode||diagnostics)presentation=_wfsopen((root/L"ship-ui.jsonl").c_str(),L"a",_SH_DENYNO);
 api=GetInterface();if(!api)return AURIE_MODULE_DEPENDENCY_NOT_RESOLVED;access.base=(uintptr_t)GetModuleHandleW(nullptr);
 // Saved P1 bindings resolve native field cells; establish the runner base first.
 ReloadControls();
 CoopBalance::Load();
 if(!qa_mode&&!diagnostics)if(auto console=GetConsoleWindow())ShowWindow(console,SW_HIDE);
 if(!ndship::signatures::Validate(access.base)){Log("REFUSING native ship hooks: game signatures differ");return AURIE_MODULE_INITIALIZATION_FAILED;}
 ndship::SetOwnerResolver(Owner);ndship::assets::RegisterOwnershipGroups();ndship::RegisterOwnedAsset(674);
 if(!ndship::InstallContextHooks(module,access.base,Log))return AURIE_MODULE_INITIALIZATION_FAILED;
 if(!Hook("Ship.PlayerCreate",0xc41ce0,(void*)PlayerCreate,(void**)&original_player_create)||!Hook("Ship.PowersCreate",0xc77c90,(void*)PowersCreate,(void**)&original_powers_create)||!Hook("Ship.PlayerStep",0xc4a650,(void*)PlayerStep,(void**)&original_player_step)||!Hook("Ship.PlayerDestroy",0xc43c40,(void*)PlayerDestroy,(void**)&original_player_destroy))return AURIE_MODULE_INITIALIZATION_FAILED;
 if(!Hook("Ship.StockEffectDraw",0x936a80,(void*)StockEffectDraw,(void**)&original_stock_effect_draw))return AURIE_MODULE_INITIALIZATION_FAILED;
 if(!Hook("Ship.HealthEffectDraw",0x88dcf0,(void*)HealthEffectDraw,(void**)&original_health_effect_draw))return AURIE_MODULE_INITIALIZATION_FAILED;
 if(!Hook("Ship.PlayerStep2",0xc5b970,(void*)PlayerStep2,(void**)&original_player_step2))return AURIE_MODULE_INITIALIZATION_FAILED;
 void* keyboard_routine=nullptr;if(!AurieSuccess(api->GetNamedRoutinePointer("keyboard_check",&keyboard_routine))||!keyboard_routine)return AURIE_MODULE_INITIALIZATION_FAILED;engine_keyboard_check=(TRoutine)keyboard_routine;
 if(!ndship::InstallQueryHooks(module,api)||!HookBuiltin("window_has_focus",Focus,original_focus))return AURIE_MODULE_INITIALIZATION_FAILED;
 if(!Hook("Ship.InputHeld",0x39a000,(void*)Held,(void**)&original_held)||!Hook("Ship.InputPressed",0x39ab60,(void*)Pressed,(void**)&original_pressed)||!Hook("Ship.InputReleased",0x39b6d0,(void*)Released,(void**)&original_released)||!Hook("Ship.GamepadHeld",0x302380,(void*)PadHeld,(void**)&original_pad_held)||!Hook("Ship.GamepadPressed",0x3038d0,(void*)PadPressed,(void**)&original_pad_pressed)||!Hook("Ship.EndScore",0x8f0f0,(void*)EndScore,(void**)&original_end_score))return AURIE_MODULE_INITIALIZATION_FAILED;
 if(!Hook("Ship.InstanceCreate",0x3a2d60,(void*)InstanceCreate,(void**)&original_instance_create))return AURIE_MODULE_INITIALIZATION_FAILED;
 if(!Hook("Ship.BlasterCreate",0x8517a0,(void*)BlasterCreate,(void**)&original_blaster_create))return AURIE_MODULE_INITIALIZATION_FAILED;
 if(!Hook("Ship.ProjectileCreate",0xc6c290,(void*)ProjectileCreate,(void**)&original_player_projectile_create)||!Hook("Ship.SaveRunToFile",0x514b50,(void*)SaveRun,(void**)&original_save_run))return AURIE_MODULE_INITIALIZATION_FAILED;
 if(!Hook("Ship.GainLevel",0x2fc6c0,(void*)GainLevel,(void**)&original_gain_level))return AURIE_MODULE_INITIALIZATION_FAILED;
 if(!Hook("Balance.CameraInit",0xbe8d0,(void*)CoopBalance::CameraInit,(void**)&CoopBalance::original_camera_init)||!Hook("Balance.CameraZoom",0xbf2e0,(void*)CoopBalance::CameraZoom,(void**)&CoopBalance::original_camera_zoom)||!Hook("Balance.ViewDimensions",0x78b460,(void*)CoopBalance::ViewDimensions,(void**)&CoopBalance::original_view_dimensions))return AURIE_MODULE_INITIALIZATION_FAILED;
 if(!Hook("Balance.ChooseTarget",0xe7a60,(void*)CoopBalance::ChooseTarget,(void**)&CoopBalance::original_choose_target)||!Hook("Balance.UpdateScore",0x781240,(void*)CoopBalance::UpdateScore,(void**)&CoopBalance::original_update_score)||!Hook("Balance.ExpControl",0x2def70,(void*)CoopBalance::ExpControl,(void**)&CoopBalance::original_exp_control))return AURIE_MODULE_INITIALIZATION_FAILED;
 if(!Hook("Balance.Curated",0x61b170,(void*)CoopBalance::Curated,(void**)&CoopBalance::original_curated)||!Hook("Balance.Dynamic",0x62df50,(void*)CoopBalance::Dynamic,(void**)&CoopBalance::original_dynamic)||!Hook("Balance.SpawnParser",0x647cd0,(void*)CoopBalance::SpawnParser,(void**)&CoopBalance::original_spawn_parser))return AURIE_MODULE_INITIALIZATION_FAILED;
 if(!ndship::lifecycle::InstallLifecycleHooks(module,access.base,{OwnedIds,Owner,Forget,DisposeList,BeginTeardown,EndTeardown,Log}))return AURIE_MODULE_INITIALIZATION_FAILED;
 if(!ndship::upgrade::Install(module,access.base,{Budget,Controller,CountUI,Log,Trays::UsesCompact,Trays::Request,SetBudget}))return AURIE_MODULE_INITIALIZATION_FAILED;
 if(!Trays::Install())return AURIE_MODULE_INITIALIZATION_FAILED;
 if(!PauseBuild::Install())return AURIE_MODULE_INITIALIZATION_FAILED;
 if(!Hook("Ship.UIFlash",0x371ea0,(void*)UIFlash,(void**)&original_ui_flash)||!Hook("Ship.UIPositions",0x4483b0,(void*)UIPositions,(void**)&original_ui_positions)||!Hook("Ship.UIRefresh",0x4c79f0,(void*)UIRefresh,(void**)&original_ui_refresh))return AURIE_MODULE_INITIALIZATION_FAILED;
 if(!Hook("Ship.HealthRect",ndship::hud::NativeDrawRectangleRva,(void*)HealthRectangle,(void**)&original_draw_rectangle))return AURIE_MODULE_INITIALIZATION_FAILED;
 if(!Hook("Ship.UpgradeTutorial",0xe60f70,(void*)UpgradeTutorial,(void**)&original_upgrade_tutorial)||!Hook("Ship.GenericTutorial",0xe5a940,(void*)GenericTutorial,(void**)&original_generic_tutorial)||!Hook("Ship.GenericTutorial2",0xe5bb10,(void*)GenericTutorial2,(void**)&original_generic_tutorial2))return AURIE_MODULE_INITIALIZATION_FAILED;
 if(!Hook("Ship.ConstructTutorial",0x11dbd0,(void*)ConstructTutorial,(void**)&original_construct_tutorial))return AURIE_MODULE_INITIALIZATION_FAILED;
 if(!Hook("Ship.SpawnTutorial",0x659ed0,(void*)SpawnTutorial,(void**)&original_spawn_tutorial))return AURIE_MODULE_INITIALIZATION_FAILED;
 if(!Hook("Ship.TutorialEffect",0xbe2030,(void*)TutorialEffect,(void**)&original_tutorial_effect))return AURIE_MODULE_INITIALIZATION_FAILED;
 if(!Hook("Ship.UpgradePrompt",0xc98890,(void*)UpgradePrompt,(void**)&original_upgrade_prompt))return AURIE_MODULE_INITIALIZATION_FAILED;
 if(!Hook("Ship.DrawGUI",0x984ec0,(void*)DrawGUI,(void**)&original_draw_gui))return AURIE_MODULE_INITIALIZATION_FAILED;
 TRoutine input_detours[]={InputBuiltin<0>,InputBuiltin<1>,InputBuiltin<2>,InputBuiltin<3>,InputBuiltin<4>,InputBuiltin<5>,InputBuiltin<6>,InputBuiltin<7>,InputBuiltin<8>,InputBuiltin<9>};
 for(int i=0;i<10;++i)if(!HookBuiltin(InputBuiltins[i],input_detours[i],original_input_builtin[i]))return AURIE_MODULE_INITIALIZATION_FAILED;
 auto status=api->CreateCallback(module,EVENT_OBJECT_CALL,(void*)CodeCallback,0);Log(AurieSuccess(status)?"full ship module ready":"full ship callback FAILED");return status;
}
