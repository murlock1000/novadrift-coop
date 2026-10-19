#pragma once
namespace CoopFlow {
ndship::flow::Rescue rescues[3];
struct BeaconVisual {unsigned long long frames=0;double x=0,y=0,radius=0;};
BeaconVisual visuals[3];
ULONGLONG last_tick=0;
void Reset(){for(auto& rescue:rescues)rescue.Reset();last_tick=0;for(auto& visual:visuals)visual={};CoopBalance::zoom.Reset();CoopBalance::last_native_width=0;}
bool Living(int owner){return Resolve(ships[owner])&&Read(Resolve(powers[owner]),nd::Fields::Hull,0)>0;}
void Down(int owner,void* self){if(ready&&owner>=1&&owner<=2){rescues[owner].Fall(Read(self,nd::Fields::X),Read(self,nd::Fields::Y));visuals[owner]={};}}
void Tick(){
 auto now=GetTickCount64();double seconds=last_tick?std::clamp((now-last_tick)/1000.0,0.0,.05):0;last_tick=now;
 if(!ready||joining||ndship::lifecycle::IsTearingDown())return;
 auto controller=Resolve(controller_id);const bool simulation_active=controller&&Read(controller,0x1b004b8,1)==0&&!ndship::upgrade::SessionActive()&&Builtin("window_has_focus").ToBoolean();
 CoopBalance::AdvanceZoom(simulation_active?seconds:0);
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
void Draw(void* self,double width,double height){
 if(!ready||ndship::upgrade::SessionActive()||Read(Resolve(controller_id),0x1b004b8,0)!=0||Read(Resolve(controller_id),0x1af7718,0)!=0||(qa_mode&&command.mutate_beacon))return;
 const double vw=Global(0x1af7468),vh=Global(0x1af7418);if(vw<=0||vh<=0)return;
 ndship::hud::detail::Painter p(api,(CInstance*)self);auto color=p.Call("draw_get_color"),alpha=p.Call("draw_get_alpha");
 for(int owner=1;owner<=2;++owner){const auto& rescue=rescues[owner];if(!rescue.down||Resolve(ships[owner]))continue;
  double x=(rescue.x-Global(0x1af7428))*width/vw,y=(rescue.y-Global(0x1af74a8))*height/vh;
  x=std::clamp(x,24.0,width-24);y=std::clamp(y,24.0,height-70);
  const double scale=std::min(width/2400.0,height/1350.0);
  const double radius=ndship::flow::Rescue::Radius*width/vw,pulse=.78+.18*std::sin(GetTickCount64()/220.0);
  // The range/progress ring sits outside a covering ship's shield, so the
  // beacon remains readable while a teammate is actually reviving it.
  p.Call("draw_set_color",{RValue(ndship::hud::PlayerColor(owner))});p.Call("draw_set_alpha",{RValue(pulse)});
  for(int i=0;i<2;++i)p.Call("draw_circle",{RValue(x),RValue(y),RValue(radius+i*scale),RValue(true)});
  static int sprite=-2;if(sprite==-2)sprite=Builtin("asset_get_index",{RValue("sSelectaWhite")}).ToInt32();
  if(sprite>=0){double w=Builtin("sprite_get_width",{RValue(sprite)}).ToDouble(),h=Builtin("sprite_get_height",{RValue(sprite)}).ToDouble();
   double ox=Builtin("sprite_get_xoffset",{RValue(sprite)}).ToDouble(),oy=Builtin("sprite_get_yoffset",{RValue(sprite)}).ToDouble(),s=64*scale/std::max(w,h);
   p.Call("draw_sprite_ext",{RValue(sprite),RValue(0),RValue(x-(w/2-ox)*s),RValue(y-(h/2-oy)*s),RValue(s),RValue(s),RValue(0),RValue(ndship::hud::PlayerColor(owner)),RValue(pulse)});
  }
  p.Call("draw_line_width",{RValue(x-10*scale),RValue(y),RValue(x+10*scale),RValue(y),RValue(3*scale)});
  p.Call("draw_line_width",{RValue(x),RValue(y-10*scale),RValue(x),RValue(y+10*scale),RValue(3*scale)});
  p.Call("draw_set_alpha",{RValue(1.0)});
  const int parts=(int)std::floor(rescue.Fraction()*40);
  for(int i=0;i<parts;++i){double a=(-90+i*9)*3.141592653589793/180,b=(a+8*3.141592653589793/180);double r=radius+7*scale;
   p.Call("draw_line_width",{RValue(x+std::cos(a)*r),RValue(y+std::sin(a)*r),RValue(x+std::cos(b)*r),RValue(y+std::sin(b)*r),RValue(5*scale)});
  }
  auto& visual=visuals[owner];++visual.frames;visual.x=x;visual.y=y;visual.radius=radius;
 }
 p.Call("draw_set_color",{color});p.Call("draw_set_alpha",{alpha});
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
  auto& visual=visuals[owner];std::fprintf(CoopBalance::trace,"{\"kind\":\"beacon_visual\",\"counter\":%d,\"owner\":%d,\"frames\":%llu,\"x\":%.8f,\"y\":%.8f,\"radius\":%.8f}\n",counter,owner,visual.frames,visual.x,visual.y,visual.radius);
 }std::fflush(CoopBalance::trace);
}
}
