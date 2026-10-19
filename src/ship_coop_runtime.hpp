#pragma once
namespace CoopFlow {
ndship::flow::Rescue rescues[3];
struct BeaconVisual {unsigned long long frames=0;double x=0,y=0,radius=0,depth=0;};
BeaconVisual visuals[3];
ULONGLONG last_tick=0;
void Reset(){for(auto& rescue:rescues)rescue.Reset();last_tick=0;for(auto& visual:visuals)visual={};CoopBalance::zoom.Reset();CoopBalance::last_native_width=0;}
bool Living(int owner){return Resolve(ships[owner])&&Read(Resolve(powers[owner]),nd::Fields::Hull,0)>0;}
void Down(int owner,void* self){if(ready&&owner>=1&&owner<=2){rescues[owner].Fall(Read(self,nd::Fields::X),Read(self,nd::Fields::Y));visuals[owner]={};}}
void Tick(){
 auto now=GetTickCount64();double seconds=last_tick?std::clamp((now-last_tick)/1000.0,0.0,.05):0;last_tick=now;
 if(!ready||joining||ndship::lifecycle::IsTearingDown())return;
 auto controller=Resolve(controller_id);const bool simulation_active=controller&&Read(controller,0x1b004b8,1)==0&&!ndship::upgrade::SessionActive()&&Builtin("window_has_focus").ToBoolean();
 // A downed teammate still occupies the arena: keep its death point reachable.
 const bool beacon_pending=(Living(1)||Living(2))&&(rescues[1].down||rescues[2].down);
 CoopBalance::AdvanceZoom(simulation_active?seconds:0,beacon_pending);
 if(!simulation_active||Read(controller,0x1af7718,0)!=0||(qa_mode&&!command.rescue_mode))return;
 const bool alive1=Living(1),alive2=Living(2);if(alive1==alive2)return;
 const int down=alive1?2:1,partner=alive1?1:2;auto ship=Resolve(ships[partner]);auto& rescue=rescues[down];
 const bool wraps=Global(0x1aeef48)!=0&&Global(0x1aef3f8)!=0;
 if(wraps){const double w=Global(0x1af7468),h=Global(0x1af7418),left=Global(0x1af7428),top=Global(0x1af74a8);if(w>0)rescue.x=left+std::fmod(std::fmod(rescue.x-left,w)+w,w);if(h>0)rescue.y=top+std::fmod(std::fmod(rescue.y-top,h)+h,h);}
 const double distance=ndship::balance::DistanceSquared(rescue.x,rescue.y,Read(ship,nd::Fields::X),Read(ship,nd::Fields::Y),wraps?Global(0x1af7468):0,wraps?Global(0x1af7418):0);
 if(rescue.Step(1.0/60.0,distance<=ndship::flow::Rescue::Radius*ndship::flow::Rescue::Radius,true)){
  if(!Respawn(down,rescue.x,rescue.y))rescue.progress=0;
 }
}
void Draw(void* self,double width,double height,double background_depth){
 if(!ready||ndship::upgrade::SessionActive()||Read(Resolve(controller_id),0x1b004b8,0)!=0||Read(Resolve(controller_id),0x1af7718,0)!=0||(qa_mode&&command.mutate_beacon))return;
 const double vw=Global(0x1af7468),vh=Global(0x1af7418);if(vw<=0||vh<=0||(!rescues[1].down&&!rescues[2].down))return;
 ndship::hud::detail::Painter p(api,(CInstance*)self);auto color=p.Call("draw_get_color"),alpha=p.Call("draw_get_alpha");
 p.Call("gpu_push_state");p.Call("gpu_set_blendenable",{RValue(true)});p.Call("gpu_set_blendmode",{RValue(0)});
 for(int owner=1;owner<=2;++owner){const auto& rescue=rescues[owner];if(!rescue.down||Resolve(ships[owner]))continue;
  double x=(rescue.x-Global(0x1af7428))*width/vw,y=(rescue.y-Global(0x1af74a8))*height/vh;
  x=std::clamp(x,24.0,width-24);y=std::clamp(y,24.0,height-70);
  // Draw in the world pass; convert the existing GUI-sized artwork to world units.
  const double units=vw/width,scale=std::min(width/2400.0,height/1350.0)*units;
  const double gui_x=x,gui_y=y;x=Global(0x1af7428)+x*units;y=Global(0x1af74a8)+y*vh/height;
  const double radius=ndship::flow::Rescue::Radius,pulse=.28+.04*std::sin(GetTickCount64()/220.0);
  // Native upgrade/construct artwork supplies the outline, glow and beacon.
  // Filling the native hex shows revival progress without hand-drawn geometry.
  struct Art {int sprite=-1;double w=0,h=0,ox=0,oy=0;};
  static std::map<std::string,Art> art;
  auto draw=[&](const char* name,double diameter,int tint,double opacity,double fraction=1.0){
   auto [it,inserted]=art.try_emplace(name);auto& a=it->second;
   if(inserted){a.sprite=Builtin("asset_get_index",{RValue(name)}).ToInt32();if(a.sprite>=0){a.w=Builtin("sprite_get_width",{RValue(a.sprite)}).ToDouble();a.h=Builtin("sprite_get_height",{RValue(a.sprite)}).ToDouble();a.ox=Builtin("sprite_get_xoffset",{RValue(a.sprite)}).ToDouble();a.oy=Builtin("sprite_get_yoffset",{RValue(a.sprite)}).ToDouble();}}
   if(a.sprite<0||a.w<=0||a.h<=0||fraction<=0)return;
   const double s=diameter/std::max(a.w,a.h);
   if(fraction<1){double cut=a.h*(1-fraction);p.Call("draw_sprite_part_ext",{RValue(a.sprite),RValue(0),RValue(0),RValue(cut),RValue(a.w),RValue(a.h-cut),RValue(x-a.w*s/2),RValue(y-a.h*s/2+cut*s),RValue(s),RValue(s),RValue(tint),RValue(opacity)});}
   else p.Call("draw_sprite_ext",{RValue(a.sprite),RValue(0),RValue(x-(a.w/2-a.ox)*s),RValue(y-(a.h/2-a.oy)*s),RValue(s),RValue(s),RValue(0),RValue(tint),RValue(opacity)});
  };
  const int accent=ndship::hud::PlayerColor(owner);
  draw("sSelectaPulse",radius*2.16,accent,.06,rescue.Fraction());
  draw("sSelectaWhite",radius*2.16,accent,pulse);
  draw("sHexTacticalBeacon",72*scale,0xffffff,.40);
  auto& visual=visuals[owner];++visual.frames;visual.x=gui_x;visual.y=gui_y;visual.radius=radius/units;visual.depth=background_depth;
 }
 p.Call("gpu_pop_state");p.Call("draw_set_color",{color});p.Call("draw_set_alpha",{alpha});
}
// Draw just after the frontmost native background, before player sprites.
// Multiple parallax backgrounds must not accumulate beacon alpha in one frame.
void DrawBackground(void* self,unsigned long long frame){
 if(!ready||(!rescues[1].down&&!rescues[2].down))return;
 static unsigned long long selected_frame=~0ULL,drawn_frame=~0ULL;static int front=-1;static double front_depth=0;
 if(selected_frame!=frame){
  selected_frame=frame;front=-1;front_depth=1e30;ndship::OwnerScope world(0);
  int asset=Asset(self),count=Builtin("instance_number",{RValue(asset)}).ToInt32();
  for(int i=0;i<count;++i){auto instance=Builtin("instance_find",{RValue(asset),RValue(i)});auto object=Resolve((int)instance.m_i64);if(!object)continue;
   RValue depth;if(!AurieSuccess(api->GetBuiltin("depth",(CInstance*)object,NULL_INDEX,depth)))continue;
   if(depth.ToDouble()<=front_depth){front=Id(object);front_depth=depth.ToDouble();}
  }
 }
 if(front!=Id(self)||drawn_frame==frame)return;drawn_frame=frame;
 Draw(self,Builtin("display_get_gui_width").ToDouble(),Builtin("display_get_gui_height").ToDouble(),front_depth);
}
// Real native collision handler, with an initialized native enemy projectile.
// Positive control after the native timer expires proves the probe can damage.
void ProbeCollision(int owner,int counter){
 if(!qa_mode||!CoopBalance::trace||owner<1||owner>2)return;
 auto ship=Resolve(ships[owner]),build=Resolve(powers[owner]);if(!ship||!build)return;
 ndship::OwnerScope scope(owner);
 auto bullet=Builtin("instance_create_depth",{RValue(Read(ship,nd::Fields::X)),RValue(Read(ship,nd::Fields::Y)),RValue(0.0),RValue(476)});auto other=Resolve((int)bullet.m_i64);if(!other)return;
 Builtin("variable_instance_set",{bullet,RValue("damage"),RValue(10.0)});Builtin("variable_instance_set",{bullet,RValue("revenge"),RValue(0.0)});
 for(auto name:{"hitPush","piercing"})Builtin("variable_instance_set",{bullet,RValue(name),RValue(0.0)});Builtin("variable_instance_set",{bullet,RValue("canDispose"),RValue(true)});
 double hull=Read(build,nd::Fields::Hull),shield=Read(build,0x1af7fd8),timer=Read(build,0x1af7f98),partner=Read(Resolve(powers[3-owner]),0x1af7f98);
 ((nd::Event)(access.base+0xc62790))(ship,other);
 std::fprintf(CoopBalance::trace,"{\"kind\":\"collision_probe\",\"counter\":%d,\"owner\":%d,\"timer\":%.8f,\"timer_after\":%.8f,\"hull\":%.8f,\"hull_after\":%.8f,\"shield\":%.8f,\"shield_after\":%.8f,\"partner_timer\":%.8f,\"partner_timer_after\":%.8f}\n",counter,owner,timer,Read(build,0x1af7f98),hull,Read(build,nd::Fields::Hull),shield,Read(build,0x1af7fd8),partner,Read(Resolve(powers[3-owner]),0x1af7f98));std::fflush(CoopBalance::trace);
 if(Resolve((int)bullet.m_i64))Builtin("instance_destroy",{bullet});
}
void Trace(int counter){
 if(!CoopBalance::trace||!ready)return;
 for(int owner=1;owner<=2;++owner){auto& r=rescues[owner];auto b=Resolve(powers[owner]);
  std::fprintf(CoopBalance::trace,"{\"kind\":\"rescue\",\"counter\":%d,\"owner\":%d,\"down\":%d,\"age\":%.8f,\"progress\":%.8f,\"x\":%.8f,\"y\":%.8f,\"invulnerability\":%.8f,\"hull\":%.8f,\"hullmax\":%.8f}\n",counter,owner,r.down,r.age,r.progress,r.x,r.y,Read(b,0x1af7f98,0),Read(b,nd::Fields::Hull,0),Read(b,0x1aeef08,0));
  auto& visual=visuals[owner];std::fprintf(CoopBalance::trace,"{\"kind\":\"beacon_visual\",\"counter\":%d,\"owner\":%d,\"frames\":%llu,\"x\":%.8f,\"y\":%.8f,\"radius\":%.8f,\"background_depth\":%.8f}\n",counter,owner,visual.frames,visual.x,visual.y,visual.radius,visual.depth);
 }std::fflush(CoopBalance::trace);
}
}
