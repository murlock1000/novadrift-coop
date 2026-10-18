#pragma once
// Engine-thread transactions for the pinned build. UI828 never survives a tick.
namespace Trays {
constexpr const char* Lists[]={"mods","gear","playerMods","gearShields","gearBodies","gearWeapons","upgradeList","wildMods","wildModsRare","wildModsVeryRare","banishMods"};
constexpr const char* Counts[]={"modCount","wildModCount","bodyCount","weaponCount","shieldCount","choiceCount"};
struct Card { RValue object;std::string title,description;int native_index=0,sprite=-1;double icon_scale=1,measure_width=-1,measure_height=0;int measure_font=-2; };
struct Tray {
 bool open=false,requested=false,inspect=false;unsigned long long epoch=0;double revision=-1;int selected=0;
 std::array<std::vector<RValue>,11> lists;std::array<RValue,6> counts;std::vector<Card> cards;
};
bool consume_confirm[3]{};
Tray trays[3];int transaction=0,broker_id=-1,transaction_owner=0;bool rerolled=false;
std::vector<int> children;unsigned long long generated=0,purchased=0,failed=0;
nd::Event original_create=nullptr;nd::Script original_finish=nullptr,original_reroll=nullptr;
TRoutine original_clear=nullptr,original_list_create=nullptr,original_list_destroy=nullptr;
std::unordered_set<int> transaction_lists;unsigned long long leaked_lists=0;
RValue Invoke(std::uintptr_t rva,void* self,std::vector<RValue> args={}) {
 RValue out;std::vector<nd::RValue*> argv;for(auto& a:args)argv.push_back((nd::RValue*)&a);
 ((nd::Script)(access.base+rva))(self,self,(nd::RValue*)&out,(int)args.size(),argv.data());return out;
}
RValue Get(void* self,const char* name){return Builtin("variable_instance_get",{Ref(Id(self)),RValue(name)});}
void Put(void* self,const char* name,const RValue& v){Builtin("variable_instance_set",{Ref(Id(self)),RValue(name),v});}
bool Busy(){return transaction!=0;}
bool IsOpen(int owner){return owner>=1&&owner<=2&&trays[owner].open;}
void Reset(){for(auto& tray:trays)tray=Tray{};consume_confirm[1]=consume_confirm[2]=false;}
bool Eligible(int owner){
 if(!ready||!globals||owner<1||owner>2||!HasShip(owner))return false;
 ndship::OwnerScope scope(owner);return Global(0x1af8938)>=4;
}
void Request(int owner){if(owner>=1&&owner<=2)trays[owner].requested=true;}
double Revision(int owner){auto p=Resolve(powers[owner]);return Global(0x1af8938)*10000000+Builtin("ds_list_size",{Field(p,0x1af7678)}).ToDouble()*10000+Read(p,0x1af96c8,0)*100+Read(p,0x1af7f88,0);}
void Capture(void* self,const char* name){
 if(!transaction||!self||!name||!std::strstr(name,"_Create_"))return;
 if(transaction==1||transaction==3){Tag(self,transaction_owner);if(Asset(self)!=828)children.push_back(Id(self));}
}
void Create(void* self,void* other){
 if(!transaction){original_create(self,other);return;}
 broker_id=Id(self);Tag(self,transaction_owner);
 if(transaction==1){original_create(self,other);return;}
 // Cleanup owns eleven independent lists, including in a commit-only broker.
 for(auto name:Lists)Put(self,name,Builtin("ds_list_create"));
}
void Dispose(){
 for(auto it=children.rbegin();it!=children.rend();++it){ndship::lifecycle::DestroyExact(*it,transaction_owner);auto ptr=ndship::lifecycle::ResolveExisting(*it);owners.erase(*it);if(ptr)ndship::UnregisterOwnedInstance(ptr);}
 children.clear();if(broker_id>=100000)ndship::lifecycle::DestroyExact(broker_id,transaction_owner);
 if(!transaction_lists.empty()){leaked_lists+=transaction_lists.size();Log("personal tray DS ownership mismatch");transaction_lists.clear();}
 broker_id=-1;transaction=0;transaction_owner=0;
}
struct Transaction {
 ndship::OwnerScope scope;
 Transaction(int owner,int mode):scope(owner){transaction=mode;transaction_owner=owner;broker_id=-1;children.clear();rerolled=false;}
 ~Transaction(){Dispose();}
 void* Make(){auto ship=Resolve(ships[transaction_owner]);auto value=Builtin("instance_create_depth",{RValue(Read(ship,nd::Fields::X,0)),RValue(Read(ship,nd::Fields::Y,0)),RValue(-100.0),RValue(828)});return Resolve((int)value.m_i64);}
};
// Roll back native generation/history/currency on a rejected transaction.
// Preserve Powers list handles: other native objects can retain their identity.
struct NativeState {
 void* build;int owner;unsigned long long epoch=generation;bool accepted=false;
 std::vector<std::pair<std::uintptr_t,RValue>> globals_before,fields_before;
 struct List {RValue handle;std::vector<RValue> values;};std::vector<List> lists_before;
 explicit NativeState(int owner,bool banish=false):build(Resolve(powers[owner])),owner(owner){
  for(auto field:ndship::build::IndependentGlobals)globals_before.emplace_back(field.cell,*(RValue*)GlobalRead(globals,access.id(field.cell)));
  for(auto cell:{0x1afa278ULL,0x1b00498ULL})fields_before.emplace_back(cell,Field(build,cell));
  if(banish)for(auto cell:{0x1afa218ULL,0x1afa2e8ULL})fields_before.emplace_back(cell,Field(build,cell));
  for(auto cell:{0x1a9de48ULL,0x1af7648ULL}){List list;list.handle=Field(build,cell);int n=Builtin("ds_list_size",{list.handle}).ToInt32();for(int i=0;i<n;++i)list.values.push_back(Builtin("ds_list_find_value",{list.handle,RValue(i)}));lists_before.push_back(std::move(list));}
 }
 ~NativeState(){if(accepted||epoch!=generation||Resolve(powers[owner])!=build)return;
  for(auto& [cell,v]:globals_before)*(RValue*)GlobalWrite(globals,access.id(cell))=v;
  for(auto& [cell,v]:fields_before)((nd::SetValue)(access.base+0xf17460))(build,access.id(cell),nd::Scalar,(nd::RValue*)&v);
  for(auto& list:lists_before){Builtin("ds_list_clear",{list.handle});for(auto& value:list.values)Builtin("ds_list_add",{list.handle,value});}
 }
};
bool Generate(int owner){
 ndship::OwnerScope scope(owner);NativeState state(owner);
 Tray next;next.epoch=generation;next.open=true;
 {Transaction tx(owner,1);auto broker=tx.Make();if(!broker){++failed;return false;}
  for(size_t i=0;i<std::size(Lists);++i){auto list=Get(broker,Lists[i]);int size=Builtin("ds_list_size",{list}).ToInt32();if(size<0||size>2048){++failed;return false;}
   for(int k=0;k<size;++k)next.lists[i].push_back(Builtin("ds_list_find_value",{list,RValue(k)}));
  }
  for(size_t i=0;i<std::size(Counts);++i)next.counts[i]=Get(broker,Counts[i]);
  for(size_t index=0;index<next.lists[6].size();++index){const auto& object=next.lists[6][index];
   if(object.m_Kind!=VALUE_REF||(int)object.m_i64==221)continue;
   Card card;card.native_index=(int)index;card.object=object;card.title=Invoke(0x350750,broker,{object}).ToString();card.description=Invoke(0x32c7d0,broker,{object}).ToString();
   auto sprite=Invoke(0x33a8a0,broker,{object});card.sprite=(int)sprite.m_i64;if(card.sprite>=0){double size=std::max(Builtin("sprite_get_width",{RValue(card.sprite)}).ToDouble(),Builtin("sprite_get_height",{RValue(card.sprite)}).ToDouble());if(size>0)card.icon_scale=42/size;}next.cards.push_back(std::move(card));
  }
  next.revision=Revision(owner);
 }
 if(qa_mode&&command.tray_fail){++failed;return false;}
 if(next.cards.empty()){++failed;Log("banked offers rejected: empty/decline selection");return false;}
 trays[owner]=std::move(next);state.accepted=true;++generated;Log(owner==2?"personal native offers generated owner=2":"personal native offers generated owner=1");return true;
}
void Fill(void* broker,const Tray& tray){
 for(size_t i=0;i<std::size(Lists);++i){auto handle=Get(broker,Lists[i]);for(const auto& item:tray.lists[i])Builtin("ds_list_add",{handle,item});}
 for(size_t i=0;i<std::size(Counts);++i)Put(broker,Counts[i],tray.counts[i]);
 Put(broker,"pos",RValue(tray.cards[tray.selected].native_index+1));Put(broker,"canConfirm",RValue(true));
}
nd::RValue* Finish(void*s,void*o,nd::RValue*r,int n,nd::RValue**a){
 if(transaction==2&&Id(s)==broker_id){auto previous=Field(Resolve(powers[transaction_owner]),0x1a9de48);if(Global(0x1af8938)>3)Builtin("ds_list_clear",{previous});SetGlobal(0x1af8938,Global(0x1af8938)+1);*(RValue*)r=RValue(false);return r;}
 return original_finish(s,o,r,n,a);
}
void ListCreate(RValue&r,CInstance*s,CInstance*o,int n,RValue*a){original_list_create(r,s,o,n,a);if(transaction)transaction_lists.insert(r.ToInt32());}
void ListDestroy(RValue&r,CInstance*s,CInstance*o,int n,RValue*a){if(transaction&&n>0)transaction_lists.erase(a[0].ToInt32());original_list_destroy(r,s,o,n,a);}
void ClearInput(RValue&r,CInstance*s,CInstance*o,int n,RValue*a){if(transaction){r=RValue(false);return;}original_clear(r,s,o,n,a);}
bool ChargeReroll(int owner){
 auto b=Resolve(powers[owner]);double count=Read(b,0x1afa278,0);
 if(!std::isfinite(count)||count<=0||Read(b,0x1afa248,0)!=0)return false;
 access.set(b,0x1afa278,count-1);SetGlobal(0x1af6bb8,Global(0x1af6bb8)+1);SetGlobal(0x1b00458,0);rerolled=true;return true;
}
nd::RValue* Reroll(void*s,void*o,nd::RValue*r,int n,nd::RValue**a){if(transaction==3&&Id(s)==broker_id){ChargeReroll(transaction_owner);*(RValue*)r=RValue(false);return r;}return original_reroll(s,o,r,n,a);}
bool Act(int owner,int action,int pick=-1){
 auto& tray=trays[owner];if(!tray.open||!Eligible(owner)||Budget(owner)<=0||tray.epoch!=generation)return false;
 ndship::OwnerScope scope(owner);if(tray.revision!=Revision(owner)){Generate(owner);return false;}
 if(pick>=0)tray.selected=pick;
 if(tray.selected<0||tray.selected>=(int)tray.cards.size())return false;
 if(action==1){
  double before=Budget(owner);{Transaction tx(owner,2);auto b=tx.Make();if(!b)return false;Fill(b,tray);Invoke(0x1053f0,b);}
  if(Budget(owner)!=before-1){++failed;return false;}
  tray=Tray{};consume_confirm[owner]=true;++purchased;return true;
 }
 if(action==2){NativeState state(owner);if(!ChargeReroll(owner))return false;state.accepted=Generate(owner);return state.accepted;}
 if(action==3){
  // Native pos<8 includes the seventh wild card, but never gear definitions.
  int asset=(int)tray.cards[tray.selected].object.m_i64;
  if(tray.cards[tray.selected].native_index>=7||asset<39||asset>229||Read(Resolve(powers[owner]),0x1af96c8,0)!=8)return false;
  NativeState state(owner,true);bool changed=false;
  {Transaction tx(owner,3);auto b=tx.Make();if(!b)return false;Fill(b,tray);Invoke(0x7aa30,b);changed=rerolled;}
  state.accepted=changed&&Generate(owner);return state.accepted;
 }
 return false;
}
void Toggle(int owner){
 auto& tray=trays[owner];tray.inspect=false;if(tray.open){tray.open=false;return;}
 if(!Eligible(owner)||Budget(owner)<=0||Read(Resolve(powers[owner]),0x1af7498,0)<=0)return;
 ndship::OwnerScope scope(owner);
 if(tray.epoch==generation&&!tray.cards.empty()&&tray.revision==Revision(owner))tray.open=true;
 else Generate(owner);
}
// Edges are sampled once on the shared Controller step. Engine keyboard state
// includes Steam Remote Play input; XInput uses the configured guest slot.
unsigned previous_bits=0;
bool BlocksInput(int owner){return IsOpen(owner)||(owner>=1&&owner<=2&&consume_confirm[owner]);}
bool KeyDown(int key){
 if(key<1||key>255)return false;
 if(qa_mode)return command.engine_input?Builtin("keyboard_check",{RValue(key)}).ToBoolean():command.key==key;
 return (GetAsyncKeyState(key)&0x8000)!=0||Builtin("keyboard_check",{RValue(key)}).ToBoolean();
}
struct Geometry {double x,y,w,row;int first;};
Geometry Layout(int owner,double width,double height){double scale=std::min(width/2400.0,height/1350.0);return {owner==1?40*scale:width-600*scale,height-220*scale,560*scale,104*scale,(trays[owner].selected/5)*5};}
void Tick(){
 if(!ready||joining||transaction||ndship::lifecycle::IsTearingDown())return;
 auto controller=Resolve(controller_id);if(!controller)return;
 for(int owner=1;owner<=2;++owner)if(!HasShip(owner))trays[owner]=Tray{};
 const auto& profile=controls.Get();unsigned pad=0;double axis=0,axis_x=0;
 if(!qa_mode&&profile.device!=ndship::config::Device::Keyboard){XINPUT_STATE p{};if(XInputGetState(profile.controller_slot,&p)==ERROR_SUCCESS){pad=p.Gamepad.wButtons;axis=p.Gamepad.sThumbLY/32768.0;axis_x=p.Gamepad.sThumbLX/32768.0;if(p.Gamepad.bRightTrigger>30)pad|=ndship::config::RightTrigger;if(p.Gamepad.bLeftTrigger>30)pad|=ndship::config::LeftTrigger;}}
 auto action=[&](ndship::config::Action a){return (profile.device!=ndship::config::Device::Controller&&KeyDown(profile.Key(a)))||(pad&profile.Pad(a))!=0;};
 unsigned bits=(KeyDown(VK_MBUTTON)?1u:0u)|(action(ndship::config::Action::Upgrade)?2u:0u)|(action(ndship::config::Action::Fire)?4u:0u)|((action(ndship::config::Action::Up)||axis>.55)?8u:0u)|((action(ndship::config::Action::Down)||axis<-.55)?16u:0u)|(action(ndship::config::Action::Stabilize)?32u:0u)|(action(ndship::config::Action::Deploy)?64u:0u)|(action(ndship::config::Action::Construct2)?128u:0u)|(Builtin("mouse_check_button",{RValue(1)}).ToBoolean()?256u:0u)|((action(ndship::config::Action::Left)||axis_x<-.55)?512u:0u)|((action(ndship::config::Action::Right)||axis_x>.55)?1024u:0u);
 unsigned pressed=bits&~previous_bits;previous_bits=bits;if(!(bits&256))consume_confirm[1]=false;if(!(bits&4))consume_confirm[2]=false;
 if(Read(controller,0x1b004b8,1)!=0||Read(controller,0x1af7718,0)!=0||!Builtin("window_has_focus").ToBoolean()||ndship::upgrade::SessionActive()||CountUI()!=0||Read(controller,ndship::build::ControllerCanLevel.cell,0)==0)return;
 double width=Builtin("display_get_gui_width").ToDouble(),height=Builtin("display_get_gui_height").ToDouble();
 double mx=Builtin("device_mouse_x_to_gui",{RValue(0)}).ToDouble(),my=Builtin("device_mouse_y_to_gui",{RValue(0)}).ToDouble();
 for(int owner=1;owner<=2;++owner){
  auto& tray=trays[owner];bool opening=tray.requested||(pressed&(owner==1?1:2));tray.requested=false;
  if(qa_mode&&command.tray==owner){command.tray=0;opening=true;}
  bool chip=owner==1&&mx<width*.24&&my>height-110*height/1350&&my<height-60*height/1350;
  if(chip&&(pressed&256))opening=true;
  if(opening){Toggle(owner);continue;}
  if(!tray.open)continue;
  tray.inspect=false;
  if(owner==1){
   if(Builtin("mouse_wheel_up").ToBoolean())tray.selected=(tray.selected+(int)tray.cards.size()-1)%(int)tray.cards.size();
   if(Builtin("mouse_wheel_down").ToBoolean())tray.selected=(tray.selected+1)%(int)tray.cards.size();
   auto g=Layout(owner,width,height);double scale=std::min(width/2400.0,height/1350.0);
   if(my>=g.y-42*scale&&my<=g.y+42*scale){
    for(int k=0;k<5;++k){int index=g.first+k;double cx=g.x+50*scale+k*g.row;
     if(index<(int)tray.cards.size()&&std::abs(mx-cx)<=40*scale){tray.selected=index;tray.inspect=true;if(pressed&256){Act(owner,1);break;}}
    }
   }else if(my>=g.y+78*scale&&my<=g.y+108*scale&&(pressed&256)){
    if(mx>=g.x+g.w*.42&&mx<=g.x+g.w*.68)Act(owner,2);
    else if(mx>=g.x+g.w*.72&&mx<=g.x+g.w&&Read(Resolve(powers[owner]),0x1af96c8,0)==8)Act(owner,3);
   }
  }
  else{
   tray.inspect=action(ndship::config::Action::Formation);
   if(pressed&(8|512))tray.selected=(tray.selected+(int)tray.cards.size()-1)%(int)tray.cards.size();
   if(pressed&(16|1024))tray.selected=(tray.selected+1)%(int)tray.cards.size();
   if(pressed&32){tray.open=false;continue;}
   if(pressed&4){Act(owner,1);continue;}
   if(pressed&64){Act(owner,2);continue;}
   if(pressed&128){Act(owner,3);continue;}
  }
  if(qa_mode&&command.tray_action&&command.owner==owner){int a=command.tray_action;command.tray_action=0;Act(owner,a,command.tray_pick);}
 }
}
// All visible shop artwork comes from the native sprite atlas. Native rich
// text resolves the owner's own stat substitutions and preserves formatting.
int Art(const char* name){static std::map<std::string,int> cache;auto it=cache.find(name);if(it!=cache.end())return it->second;int value=Builtin("asset_get_index",{RValue(name)}).ToInt32();cache[name]=value;return value;}
struct SpriteSize{double w,h,ox,oy;};
SpriteSize Size(int sprite){static std::map<int,SpriteSize> cache;auto it=cache.find(sprite);if(it!=cache.end())return it->second;SpriteSize size{Builtin("sprite_get_width",{RValue(sprite)}).ToDouble(),Builtin("sprite_get_height",{RValue(sprite)}).ToDouble(),Builtin("sprite_get_xoffset",{RValue(sprite)}).ToDouble(),Builtin("sprite_get_yoffset",{RValue(sprite)}).ToDouble()};cache[sprite]=size;return size;}
void Sprite(ndship::hud::detail::Painter& p,int sprite,double x,double y,double diameter,int color,double alpha=1){if(sprite<0)return;auto size=Size(sprite);double scale=diameter/std::max(size.w,size.h);p.Call("draw_sprite_ext",{RValue(sprite),RValue(0),RValue(x-(size.w/2-size.ox)*scale),RValue(y-(size.h/2-size.oy)*scale),RValue(scale),RValue(scale),RValue(0),RValue(color),RValue(alpha)});}
void NativeBox(ndship::hud::detail::Painter& p,double x,double y,double w,double h,double){
 // Same translucent background sprite used by native oUpgrade DrawGUI.
 int sprite=Art("sTutorialBox");if(sprite<0)return;auto size=Size(sprite);double sx=w/size.w,sy=h/size.h;
 p.Call("draw_sprite_ext",{RValue(sprite),RValue(0),RValue(x+size.ox*sx),RValue(y+size.oy*sy),RValue(sx),RValue(sy),RValue(0),RValue(0xffffff),RValue(1.0)});
}
std::string Binding(ndship::config::Action action){const auto& profile=controls.Get();return profile.device==ndship::config::Device::Controller?ndship::config::PadLabel(profile.Pad(action)):ndship::config::KeyLabel(profile.Key(action));}
void Draw(void* self,double width,double height){
 if(!ready||ndship::upgrade::SessionActive()||Read(Resolve(controller_id),0x1b004b8,1)!=0||Read(Resolve(controller_id),0x1af7718,0)!=0)return;
 ndship::hud::detail::Painter p(api,(CInstance*)self);auto color=p.Call("draw_get_color"),alpha=p.Call("draw_get_alpha"),align=p.Call("draw_get_halign"),valign=p.Call("draw_get_valign");p.Call("draw_set_valign",{RValue(0)});
 const double scale=std::min(width/2400.0,height/1350.0);int font=p.Call("draw_get_font").ToInt32();p.SetLayoutFont(font);double text_scale=font<0?2*scale:1;
 for(int owner=1;owner<=2;++owner){
  if(!Eligible(owner)||Budget(owner)<=0)continue;
  int accent=ndship::hud::PlayerColor(owner);double chipx=owner==1?40*scale:width-600*scale;
  p.FittedText(chipx,height-100*scale,"+ "+std::to_string((int)Budget(owner))+"  "+(owner==1?"Mouse middle":Binding(ndship::config::Action::Upgrade)),accent,text_scale,520*scale);
  auto& tray=trays[owner];if(!tray.open||tray.cards.empty())continue;
  auto g=Layout(owner,width,height);
  for(int index=g.first;index<std::min((int)tray.cards.size(),g.first+5);++index){double x=g.x+50*scale+(index-g.first)*g.row;
   Sprite(p,tray.cards[index].sprite,x,g.y,68*scale,0xffffff);
   if(index==tray.selected){int frame=Art("sSelectaWhite");auto a=Size(tray.cards[index].sprite),b=Size(frame);Sprite(p,frame,x,g.y,136*scale*std::max(b.w,b.h)/std::max(a.w,a.h),accent,.85);}
  }
  auto& card=tray.cards[tray.selected];
  p.FittedText(g.x,g.y+48*scale,card.title+"  "+std::to_string(tray.selected+1)+"/"+std::to_string(tray.cards.size()),accent,text_scale*.85,g.w);
  const auto rerolls=std::to_string((int)Read(Resolve(powers[owner]),0x1afa278,0));bool researcher=Read(Resolve(powers[owner]),0x1af96c8,0)==8;
  if(owner==1){
   p.FittedText(g.x,g.y+84*scale,"Scroll / hover",accent,text_scale*.65,g.w*.4);
   p.FittedText(g.x+g.w*.45,g.y+84*scale,"Reroll "+rerolls,accent,text_scale*.65,g.w*.24);
   if(researcher)p.FittedText(g.x+g.w*.75,g.y+84*scale,"Banish",accent,text_scale*.65,g.w*.25);
  }else{
   std::string footer=Binding(ndship::config::Action::Fire)+" choose | "+Binding(ndship::config::Action::Formation)+" inspect | "+Binding(ndship::config::Action::Deploy)+" reroll "+rerolls;
   if(researcher)footer+=" | "+Binding(ndship::config::Action::Construct2)+" banish";
   p.FittedText(g.x,g.y+84*scale,footer,accent,text_scale*.65,g.w);
  }
  if(tray.inspect){
   ndship::OwnerScope scope(owner);double sx=text_scale*.85,available=g.w-80*scale;
   if(card.measure_width!=available||card.measure_font!=font){auto metrics=Invoke(0x206330,Resolve(powers[owner]),{RValue(g.x),RValue(g.y),RValue(card.description),RValue(0.0),RValue(available/sx),RValue(0.0),RValue(sx),RValue(sx)});card.measure_height=metrics.IsArray()?metrics[(size_t)2].ToDouble():0;card.measure_width=available;card.measure_font=font;}
   double info_height=std::min(300*scale,std::max(72*scale,card.measure_height+36*scale));
   double x=g.x,y=g.y-info_height-55*scale;
   NativeBox(p,x,y,g.w,info_height,scale);
   if(card.measure_height>info_height-32*scale)sx*=(info_height-32*scale)/card.measure_height;
   Invoke(0x206330,Resolve(powers[owner]),{RValue(x+44*scale),RValue(y+16*scale),RValue(card.description),RValue(1.0),RValue(available/sx),RValue(0.0),RValue(sx),RValue(sx)});
  }
 }
 p.Call("draw_set_color",{color});p.Call("draw_set_alpha",{alpha});p.Call("draw_set_halign",{align});p.Call("draw_set_valign",{valign});
}
// Verified presentation-only entry detours; active only during a transaction.
nd::Script presentation_original[8]{};
template<int I>nd::RValue* Presentation(void*s,void*o,nd::RValue*r,int n,nd::RValue**a){if(transaction){*(RValue*)r=RValue(false);return r;}return presentation_original[I](s,o,r,n,a);}
void Tick();void Draw(void*,double,double);
void Trace(int counter){
 if(!CoopBalance::trace||!ready||transaction)return;
 for(int owner=1;owner<=2;++owner){auto& tray=trays[owner];ndship::OwnerScope scope(owner);
  std::fprintf(CoopBalance::trace,"{\"kind\":\"tray\",\"counter\":%d,\"owner\":%d,\"open\":%d,\"stage\":%.0f,\"budget\":%.0f,\"rerolls\":%.0f,\"generated\":%llu,\"purchased\":%llu,\"failed\":%llu,\"cards\":[",counter,owner,tray.open,Global(0x1af8938),Budget(owner),Read(Resolve(powers[owner]),0x1afa278,0),generated,purchased,failed);
  for(size_t i=0;i<tray.cards.size();++i)std::fprintf(CoopBalance::trace,"%s%d",i?",":"",(int)tray.cards[i].object.m_i64);
  std::fprintf(CoopBalance::trace,"],\"leaked_lists\":%llu,\"body\":%.0f,\"weapon\":%d,\"mod_count\":%d,\"acquired\":[",leaked_lists,Read(Resolve(powers[owner]),0x1af96c8), (int)Field(Resolve(powers[owner]),0x1af7898).m_i64,Builtin("ds_list_size",{Field(Resolve(powers[owner]),0x1af7678)}).ToInt32());
  auto mods=Field(Resolve(powers[owner]),0x1af7678);int n=Builtin("ds_list_size",{mods}).ToInt32();for(int i=0;i<n;++i){auto v=Builtin("ds_list_find_value",{mods,RValue(i)});std::fprintf(CoopBalance::trace,"%s%d",i?",":"",v.m_Kind==VALUE_REF?(int)v.m_i64:v.ToInt32());}
  int helpers=0;for(int id:OwnedIds(owner)){int asset=Asset(Resolve(id));if(asset==537||asset==538)++helpers;}
  std::fprintf(CoopBalance::trace,"],\"shield_type\":%.0f,\"helix_helpers\":%d,\"bans\":[",Read(Resolve(powers[owner]),0x1af7f88),helpers);
  auto bans=Field(Resolve(powers[owner]),0x1af7648);n=Builtin("ds_list_size",{bans}).ToInt32();for(int i=0;i<n;++i){auto v=Builtin("ds_list_find_value",{bans,RValue(i)});std::fprintf(CoopBalance::trace,"%s%d",i?",":"",v.m_Kind==VALUE_REF?(int)v.m_i64:v.ToInt32());}
  auto enemy=Builtin("instance_find",{RValue(246),RValue(0)});auto e=Resolve((int)enemy.m_i64);
  std::fprintf(CoopBalance::trace,"],\"enemy\":%d,\"enemy_x\":%.8f,\"enemy_y\":%.8f}\n",Id(e),Read(e,nd::Fields::X,0),Read(e,nd::Fields::Y,0));
 }std::fflush(CoopBalance::trace);
}

bool Install(){
 struct Entry{const char*name;std::uintptr_t rva;std::array<unsigned char,16> signature;void* detour;void** original;};
 const Entry entries[]={
 {"Tray.setupDisplay",0x57b370,{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x48,0x89,0x7c,0x24,0x20,0x4c},(void*)Presentation<0>, (void**)&presentation_original[0]},
 {"Tray.drawHexes",0x1cc8f0,{0x48,0x8b,0xc4,0x48,0x89,0x58,0x20,0x4c,0x89,0x40,0x18,0x48,0x89,0x50,0x10,0x48},(void*)Presentation<1>, (void**)&presentation_original[1]},
 {"Tray.drawOwnedGear",0x1d3200,{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x48,0x89,0x7c,0x24,0x18,0x4c},(void*)Presentation<2>, (void**)&presentation_original[2]},
 {"Tray.drawOwnedMods",0x1d3660,{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x48,0x89,0x7c,0x24,0x18,0x55},(void*)Presentation<3>, (void**)&presentation_original[3]},
 {"Tray.drawBanishedMods",0x1b0870,{0x48,0x8b,0xc4,0x48,0x89,0x58,0x20,0x4c,0x89,0x40,0x18,0x48,0x89,0x50,0x10,0x48},(void*)Presentation<4>, (void**)&presentation_original[4]},
 {"Tray.moveSelecta",0x40c8d0,{0x48,0x8b,0xc4,0x48,0x89,0x58,0x20,0x48,0x89,0x50,0x10,0x48,0x89,0x48,0x08,0x55},(void*)Presentation<5>, (void**)&presentation_original[5]},
 {"Tray.triggerSaveRun",0x535320,{0x48,0x8b,0xc4,0x48,0x89,0x58,0x18,0x48,0x89,0x50,0x10,0x48,0x89,0x48,0x08,0x55},(void*)Presentation<6>, (void**)&presentation_original[6]},
 {"Tray.saveWindowPosition",0x5449a0,{0x48,0x8b,0xc4,0x48,0x89,0x58,0x08,0x48,0x89,0x70,0x10,0x48,0x89,0x78,0x18,0x55},(void*)Presentation<7>, (void**)&presentation_original[7]},
 {"Tray.finishUpgrade",0x2f2600,{0x48,0x8b,0xc4,0x48,0x89,0x58,0x20,0x48,0x89,0x50,0x10,0x48,0x89,0x48,0x08,0x55},(void*)Finish, (void**)&original_finish},
 {"Tray.rerollUpgrade",0x4e17d0,{0x48,0x8b,0xc4,0x48,0x89,0x58,0x20,0x4c,0x89,0x40,0x18,0x48,0x89,0x50,0x10,0x48},(void*)Reroll, (void**)&original_reroll},
 {"Tray.oUpgrade_Create_0",0xe8d880,{0x48,0x89,0x54,0x24,0x10,0x48,0x89,0x4c,0x24,0x08,0x55,0x53,0x56,0x57,0x41,0x54},(void*)Create, (void**)&original_create},
 };
 for(const auto& e:entries)if(std::memcmp((void*)(access.base+e.rva),e.signature.data(),16)){Log("REFUSING personal trays: native signature mismatch");return false;}
 for(const auto& e:entries)if(!Hook(e.name,e.rva,e.detour,e.original))return false;
 return HookBuiltin("io_clear",ClearInput,original_clear)&&HookBuiltin("ds_list_create",ListCreate,original_list_create)&&HookBuiltin("ds_list_destroy",ListDestroy,original_list_destroy);
}
}
