#pragma once
// Included inside the module's anonymous namespace after its native accessors.
namespace CoopBalance {
ndship::balance::Profile profile;
nd::Script original_camera_init=nullptr,original_camera_zoom=nullptr,original_view_dimensions=nullptr;
nd::Script original_choose_target=nullptr,original_update_score=nullptr,original_exp_control=nullptr;
thread_local int camera_scope=0,team_level_scope=0,wave_scope=0;
nd::Script original_curated=nullptr,original_dynamic=nullptr,original_spawn_parser=nullptr;
double spawn_carry=0;
bool qa_xp_award=false;
bool qa_probe_running=false;
void Probe(int mode,int counter);
std::unordered_map<int,int> targets;
std::array<int,1024> hostile_cache{};
FILE* trace=nullptr;
unsigned long long ai_calls[3]{},xp_calls=0,arena_updates=0;
double last_native_width=0,last_native_height=0,last_arena_width=0,last_arena_height=0;
bool Active(){return profile.enabled&&ready&&!joining&&!ndship::lifecycle::IsTearingDown();}
int Living(){return int(Resolve(ships[1])!=nullptr)+int(Resolve(ships[2])!=nullptr);}
bool Hostile(void* self){
 int asset=Asset(self);if(asset<0||asset>=1024||Owner(self)>0||ndship::assets::IsOwned(asset))return false;
 auto& cached=hostile_cache[asset];if(!cached){
  bool enemy=false;for(int parent:{483,476,450,447,464,472,478,482})
   if(asset==parent||Builtin("object_is_ancestor",{RValue(asset),RValue(parent)}).ToBoolean()){enemy=true;break;}
  cached=enemy?1:-1;
 }
 return cached==1;
}
int Select(void* self){
 if(!Active()||!Hostile(self))return 0;
 auto p1=Resolve(ships[1]),p2=Resolve(ships[2]);bool alive1=p1!=nullptr,alive2=p2!=nullptr;
 if(qa_mode&&command.balance_mutate)return alive1?1:(alive2?2:0);
 // Native chooseTarget still handles decoys/cloak. Prefer the visible ship
 // when the other is cloaked; do not reveal both-cloaked ships ourselves.
 RValue visible1(true),visible2(true);if(p1)api->GetBuiltin("visible",(CInstance*)p1,NULL_INDEX,visible1);if(p2)api->GetBuiltin("visible",(CInstance*)p2,NULL_INDEX,visible2);
 const bool cloak1=alive1&&(!visible1.ToBoolean()||Read(Resolve(powers[1]),0x1afbb78,0)!=0);
 const bool cloak2=alive2&&(!visible2.ToBoolean()||Read(Resolve(powers[2]),0x1afbb78,0)!=0);
 if(cloak1!=cloak2){alive1=alive1&&!cloak1;alive2=alive2&&!cloak2;}
 const double x=Read(self,nd::Fields::X),y=Read(self,nd::Fields::Y);
 const bool wraps=Global(0x1aeef48)!=0&&Global(0x1aef3f8)!=0;
 const double w=wraps?Global(0x1af7468):0,h=wraps?Global(0x1af7418):0;
 const double d1=p1?ndship::balance::DistanceSquared(x,y,Read(p1,nd::Fields::X),Read(p1,nd::Fields::Y),w,h):0;
 const double d2=p2?ndship::balance::DistanceSquared(x,y,Read(p2,nd::Fields::X),Read(p2,nd::Fields::Y),w,h):0;
 auto& previous=targets[Id(self)];previous=ndship::balance::Target(d1,d2,alive1,alive2,previous);return previous;
}
void ScaleCamera(void* self){
 if(!Active()||profile.arena_scale==1)return;
 RValue camera;if(!AurieSuccess(api->GetBuiltin("view_camera",(CInstance*)self,0,camera)))return;
 const double w=Builtin("camera_get_view_width",{camera},self).ToDouble(),h=Builtin("camera_get_view_height",{camera},self).ToDouble();
 const double x=Builtin("camera_get_view_x",{camera},self).ToDouble(),y=Builtin("camera_get_view_y",{camera},self).ToDouble();
 if(!(w>0&&h>0&&std::isfinite(w)&&std::isfinite(h)))return;
 last_native_width=w;last_native_height=h;last_arena_width=w*profile.arena_scale;last_arena_height=h*profile.arena_scale;
 Builtin("camera_set_view_size",{camera,RValue(last_arena_width),RValue(last_arena_height)},self);
 Builtin("camera_set_view_pos",{camera,RValue(x-(last_arena_width-w)/2),RValue(y-(last_arena_height-h)/2)},self);++arena_updates;
}
nd::RValue* ViewDimensions(void*s,void*o,nd::RValue*r,int n,nd::RValue**a){
 if(camera_scope)ScaleCamera(s);return original_view_dimensions(s,o,r,n,a);
}
nd::RValue* CameraZoom(void*s,void*o,nd::RValue*r,int n,nd::RValue**a){
 ++camera_scope;auto out=original_camera_zoom(s,o,r,n,a);--camera_scope;return out;
}
nd::RValue* CameraInit(void*s,void*o,nd::RValue*r,int n,nd::RValue**a){
 auto out=original_camera_init(s,o,r,n,a);if(Active()){ScaleCamera(s);nd::RValue ignored{};ignored.kind=5;original_view_dimensions(s,o,&ignored,0,nullptr);}return out;
}
nd::RValue* ChooseTarget(void*s,void*o,nd::RValue*r,int n,nd::RValue**a){
 int selected=Select(s);ndship::TargetScope target(selected);
 ndship::OwnerScope native_queries(selected?selected:ndship::active_owner);
 auto out=original_choose_target(s,o,r,n,a);
 if(selected){
  ++ai_calls[selected];auto actual=Field(s,nd::Fields::Target);
  if(trace&&qa_probe_running)std::fprintf(trace,"{\"kind\":\"native_target\",\"selected\":%d,\"raw_kind\":%d,\"raw_payload\":%llu,\"native_x\":%.8f,\"native_y\":%.8f}\n",selected,actual.m_Kind,(unsigned long long)actual.m_i64,Read(s,0x1aff038),Read(s,0x1aff018));
  if(ndship::detail::AssetIndex((nd::RValue*)&actual)==603&&Resolve(ships[selected])){
   auto reference=Ref(ships[selected]);((nd::SetValue)(access.base+0xf17460))(s,access.id(nd::Fields::Target),nd::Scalar,(nd::RValue*)&reference);
  }
 }
 return out;
}
double XPMultiplier(){return Active()&&Living()>1?profile.xp_per_enemy:1.0;}
nd::RValue* UpdateScore(void*s,void*o,nd::RValue*r,int n,nd::RValue**a){
 double raw=0;const double multiplier=XPMultiplier();
 const int pickup=Asset(s);
 if((pickup!=284&&pickup!=286&&pickup!=289&&!qa_xp_award)||multiplier==1||n<1||n>16||!a||!a[0]||!ndship::build::Number(*a[0],raw)||raw<=0)return original_update_score(s,o,r,n,a);
 nd::RValue value{};value.real=raw*multiplier;nd::RValue* args[16]{};for(int i=0;i<n;++i)args[i]=a[i];args[0]=&value;
 auto controller=Resolve(controller_id);const double before=Read(controller,0x1a9da98,0);
 auto out=original_update_score(s,o,r,n,args);++xp_calls;
 if(trace){std::fprintf(trace,"{\"kind\":\"xp\",\"raw\":%.8f,\"scaled\":%.8f,\"before\":%.8f,\"after\":%.8f,\"collector_owner\":%d}\n",raw,value.real,before,Read(controller,0x1a9da98,0),ndship::active_owner);std::fflush(trace);}
 return out;
}
nd::RValue* ExpControl(void*s,void*o,nd::RValue*r,int n,nd::RValue**a){
 ++team_level_scope;auto out=original_exp_control(s,o,r,n,a);--team_level_scope;return out;
}
bool Ordinary(int asset){switch(asset){case 293:case 294:case 333:case 543:case 544:case 552:case 789:return true;default:return false;}}
nd::RValue* Curated(void*s,void*o,nd::RValue*r,int n,nd::RValue**a){++wave_scope;auto out=original_curated(s,o,r,n,a);--wave_scope;return out;}
nd::RValue* Dynamic(void*s,void*o,nd::RValue*r,int n,nd::RValue**a){++wave_scope;auto out=original_dynamic(s,o,r,n,a);--wave_scope;return out;}
nd::RValue* SpawnParser(void*s,void*o,nd::RValue*r,int n,nd::RValue**a){
 auto out=original_spawn_parser(s,o,r,n,a);
 if(!wave_scope||!Active()||Living()<2||Asset(s)!=482)return out;
 auto enemy=Field(s,0x1b01ad8);int asset=ndship::detail::AssetIndex((nd::RValue*)&enemy);
 const double count=Read(s,0x1b01b28,0);
 if(!Ordinary(asset)||count<1||count>128||std::floor(count)!=count)return out;
 const double budget=count*profile.enemy_budget+spawn_carry;const double scaled=std::floor(budget+1e-9);spawn_carry=budget-scaled;
 access.set(s,0x1b01b28,scaled);
 if(trace){std::fprintf(trace,"{\"kind\":\"spawn\",\"wave\":%.0f,\"asset\":%d,\"native_count\":%.0f,\"tuned_count\":%.0f,\"carry\":%.8f}\n",Builtin("variable_global_get",{RValue("wave")}).ToDouble(),asset,count,scaled,spawn_carry);std::fflush(trace);}
 return out;
}
void Load(){
 std::ifstream file(root/ndship::balance::FileName);if(file){std::string text((std::istreambuf_iterator<char>(file)),{}),error;
  if(!ndship::balance::Parse(text,profile,error)){profile.enabled=false;Log(error.c_str());Log("Co-op balance disabled: fix profile and restart");}}
 else {std::ofstream file(root/ndship::balance::FileName);file<<ndship::balance::Defaults();}
 if(qa_mode||diagnostics)trace=_wfsopen((root/L"ship-balance.jsonl").c_str(),L"a",_SH_DENYNO);
 char line[220];std::snprintf(line,sizeof line,"Co-op balance enabled=%d arena=%.2f density=%.2f XP=%.2f",profile.enabled,profile.arena_scale,profile.enemy_budget,profile.xp_per_enemy);Log(line);
}
void State(int counter){
 if(!trace||!ready||!globals)return;
 double choices[3]{},effective[3]{},added[3]{};
 for(int owner=1;owner<=2;++owner){ndship::OwnerScope scope(owner);choices[owner]=Global(0x1af8ce8);effective[owner]=Global(0x1af98c8);added[owner]=Global(0x1af8bc8);}
 std::fprintf(trace,"{\"kind\":\"state\",\"counter\":%d,\"level\":%.8f,\"base_level\":%.8f,\"choices1\":%.8f,\"choices2\":%.8f,\"effective1\":%.8f,\"effective2\":%.8f,\"added1\":%.8f,\"added2\":%.8f,\"width\":%.8f,\"height\":%.8f,\"ai1\":%llu,\"ai2\":%llu,\"living\":%d}\n",counter,Global(0x1af9148),Global(0x1afd8f8),choices[1],choices[2],effective[1],effective[2],added[1],added[2],Global(0x1af7468),Global(0x1af7418),ai_calls[1],ai_calls[2],Living());std::fflush(trace);
}
}
