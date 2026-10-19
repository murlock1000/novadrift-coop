#pragma once
// Two native pause inventories. Persistent icons, frames and hover information
// are created by the original game, with temporary owner/layout context only.
namespace PauseBuild {
bool creating=false;
int capture_owner=0,stats_owner=1;
bool previous_tab=false,previous_click=false;
nd::Script original_pause=nullptr,original_hover=nullptr,original_draw_pause=nullptr;
std::vector<int> graphics[3];
double panel_x[3]{},panel_y=0;
constexpr std::uintptr_t LayoutCells[]={0x1b07168,0x1b07118,0x1b07108,0x1b07148,0x1b07128,
 0x1b06ae8,0x1b06ab8,0x1b06ac8,0x1b06ad8,0x1b06aa8,0x1af4908};
void Capture(void* self,const char* name){
 if(!capture_owner||!self||!name||!std::strstr(name,"_Create_"))return;
 if(Asset(self)==513||Asset(self)==514){Tag(self,capture_owner);graphics[capture_owner].push_back(Id(self));}
}
class LayoutScope {
 void* controller_;ndship::OwnerScope owner_;
 RValue list_,bans_,scale_;std::array<RValue,std::size(LayoutCells)> fields_;
public:
 LayoutScope(void* controller,int owner):controller_(controller),owner_(owner){
  list_=Field(controller,0x1af7678);bans_=Field(controller,0x1af7648);scale_=*(RValue*)GlobalRead(globals,access.id(0x1b02718));
  for(int i=0;i<std::size(LayoutCells);++i)fields_[i]=Field(controller,LayoutCells[i]);
  const double native_scale=scale_.ToDouble();double factor=.75;
  const double width=Builtin("display_get_gui_width").ToDouble(),height=Builtin("display_get_gui_height").ToDouble();
  const double count=Builtin("ds_list_size",{Field(Resolve(powers[owner]),0x1af7678)}).ToDouble();
  const double ban_count=Builtin("ds_list_size",{Field(Resolve(powers[owner]),0x1af7648)}).ToDouble();
  for(int i=0;i<20;++i){double row=std::max(1.0,fields_[4].ToDouble()*factor);
   double rows=std::max(1.0,std::floor((height-250*native_scale)/row)-3);
   double ban_rows=std::max(1.0,std::floor((height-250*native_scale)/std::max(1.0,fields_[9].ToDouble()*factor)));
   double need=(std::ceil(count/rows)+1.5)*fields_[2].ToDouble()+(std::ceil(ban_count/ban_rows)+1)*fields_[7].ToDouble();
   if(need*factor<=width*.22)break;factor*=.9;}
  panel_x[owner]=owner==1?48*native_scale:width*.74;
  panel_y=112*native_scale;
  access.set(controller,LayoutCells[0],panel_x[owner]);access.set(controller,LayoutCells[1],panel_y);
  for(int i=2;i<5;++i)access.set(controller,LayoutCells[i],fields_[i].ToDouble()*factor);
  access.set(controller,LayoutCells[5],(owner==1?width*.26:width)-48*native_scale);access.set(controller,LayoutCells[6],panel_y);
  for(int i=7;i<std::size(LayoutCells);++i)access.set(controller,LayoutCells[i],fields_[i].ToDouble()*factor);
  SetGlobal(0x1b02718,native_scale*factor);
  auto list=Field(Resolve(powers[owner]),0x1af7678);
  ((nd::SetValue)(access.base+0xf17460))(controller,access.id(0x1af7678),nd::Scalar,(nd::RValue*)&list);
  auto bans=Field(Resolve(powers[owner]),0x1af7648);
  ((nd::SetValue)(access.base+0xf17460))(controller,access.id(0x1af7648),nd::Scalar,(nd::RValue*)&bans);
 }
 ~LayoutScope(){
  ((nd::SetValue)(access.base+0xf17460))(controller_,access.id(0x1af7678),nd::Scalar,(nd::RValue*)&list_);
  ((nd::SetValue)(access.base+0xf17460))(controller_,access.id(0x1af7648),nd::Scalar,(nd::RValue*)&bans_);
  for(int i=0;i<std::size(LayoutCells);++i)((nd::SetValue)(access.base+0xf17460))(controller_,access.id(LayoutCells[i]),nd::Scalar,(nd::RValue*)&fields_[i]);
  *(RValue*)GlobalWrite(globals,access.id(0x1b02718))=scale_;
 }
};
nd::RValue* Owned(nd::Script original,void*s,void*o,nd::RValue*r,int n,nd::RValue**a){
 if(!ready||!creating||(qa_mode&&command.mutate_pause))return original(s,o,r,n,a);
 for(int owner=1;owner<=2;++owner){
  if(!Resolve(powers[owner]))continue;
  LayoutScope layout(s,owner);capture_owner=owner;original(s,o,r,n,a);capture_owner=0;
 }
 return r;
}
nd::RValue* Pause(void*s,void*o,nd::RValue*r,int n,nd::RValue**a){
 if(!ready)return original_pause(s,o,r,n,a);
 ndship::OwnerScope canonical(1);
 graphics[1].clear();graphics[2].clear();creating=true;stats_owner=1;
 previous_tab=Trays::KeyDown(VK_TAB);previous_click=Builtin("mouse_check_button",{RValue(1)}).ToBoolean();
 auto result=original_pause(s,o,r,n,a);creating=false;capture_owner=0;
 if(CoopBalance::trace)for(int owner=1;owner<=2;++owner){
  auto b=Resolve(powers[owner]);auto mods=Field(b,0x1af7678);
  std::fprintf(CoopBalance::trace,"{\"kind\":\"pause_build\",\"counter\":%d,\"owner\":%d,\"x\":%.3f,\"y\":%.3f,\"mods\":[",command.counter,owner,panel_x[owner],panel_y);
  int count=Builtin("ds_list_size",{mods}).ToInt32();for(int i=0;i<count;++i)std::fprintf(CoopBalance::trace,"%s%d",i?",":"",(int)Builtin("ds_list_find_value",{mods,RValue(i)}).m_i64);
  std::fprintf(CoopBalance::trace,"],\"graphics\":[");bool first=true;
  for(int id:graphics[owner])if(auto p=Resolve(id)){
   RValue sprite;api->GetBuiltin("sprite_index",(CInstance*)p,NULL_INDEX,sprite);
   std::fprintf(CoopBalance::trace,"%s{\"id\":%d,\"sprite\":%d,\"x\":%.3f,\"y\":%.3f}",first?"":",",id,sprite.ToInt32(),Read(p,nd::Fields::X,0),Read(p,nd::Fields::Y,0));first=false;
  }
  std::fprintf(CoopBalance::trace,"]}\n");std::fflush(CoopBalance::trace);
 }
 return result;
}
nd::RValue* Hover(void*s,void*o,nd::RValue*r,int n,nd::RValue**a){
 if(!ready||Read(Resolve(controller_id),0x1b004b8,0)==0)return original_hover(s,o,r,n,a);
 // Native hit testing uses each persistent graphic's mask. Contextualize only
 // the description/stat substitutions for the panel under the mouse.
 const double x=Builtin("device_mouse_x_to_gui",{RValue(0)}).ToDouble();
 ndship::OwnerScope scope(x>Builtin("display_get_gui_width").ToDouble()*.5?2:1);
 return original_hover(s,o,r,n,a);
}
nd::RValue* Stats(void*s,void*o,nd::RValue*r,int n,nd::RValue**a){
 if(!ready)return original_draw_pause(s,o,r,n,a);
 // Draw64 is rendered to two surfaces. Reproduce the same native scroll and
 // rainbow frame in both, while committing its shared display state once.
 constexpr std::uintptr_t cells[]={0x1b07b68,0x1b07638,0x1aff418,0x1b07dd8};
 static unsigned long long frame=~0ULL;
 static std::array<RValue,4> before,after;
 const auto current=input_frame.load(std::memory_order_relaxed);const bool second=frame==current;
 if(!second){frame=current;for(int i=0;i<4;++i)before[i]=Field(s,cells[i]);}
 else for(int i=0;i<4;++i)((nd::SetValue)(access.base+0xf17460))(s,access.id(cells[i]),nd::Scalar,(nd::RValue*)&before[i]);
 ndship::OwnerScope scope(stats_owner);
 const bool previous=pause_stats_rendering;pause_stats_rendering=true;
 auto result=original_draw_pause(s,o,r,n,a);pause_stats_rendering=previous;
 if(!second)for(int i=0;i<4;++i)after[i]=Field(s,cells[i]);
 else for(int i=0;i<4;++i)((nd::SetValue)(access.base+0xf17460))(s,access.id(cells[i]),nd::Scalar,(nd::RValue*)&after[i]);
 if(CoopBalance::trace){static ULONGLONG at=0;auto now=GetTickCount64();if(now-at>=100){at=now;
  auto b=Resolve(powers[stats_owner]);std::fprintf(CoopBalance::trace,"{\"kind\":\"pause_stats\",\"counter\":%d,\"owner\":%d,\"body\":%.0f,\"weapon\":%d,\"shield_type\":%.0f}\n",command.counter,stats_owner,Read(b,0x1af96c8),(int)Field(b,0x1af7898).m_i64,Read(b,0x1af7f88));std::fflush(CoopBalance::trace);
 }}
 return result;
}
void Draw(void*self,double width,double){
 if(!ready||Read(Resolve(controller_id),0x1b004b8,0)==0)return;
 const bool tab=Trays::KeyDown(VK_TAB),click=Builtin("mouse_check_button",{RValue(1)}).ToBoolean();
 if(tab&&!previous_tab)stats_owner=3-stats_owner;
 if(click&&!previous_click){double x=Builtin("device_mouse_x_to_gui",{RValue(0)}).ToDouble(),y=Builtin("device_mouse_y_to_gui",{RValue(0)}).ToDouble();
  if(y<panel_y+25*Global(0x1b02718)){if(x<width*.28)stats_owner=1;else if(x>width*.72)stats_owner=2;}}
 previous_tab=tab;previous_click=click;
 ndship::hud::detail::Painter p(api,(CInstance*)self);
 auto color=p.Call("draw_get_color"),alpha=p.Call("draw_get_alpha"),font=p.Call("draw_get_font"),align=p.Call("draw_get_halign"),valign=p.Call("draw_get_valign");
 p.Call("draw_set_font",{RValue((int)Global(0x1b059b8))});p.Call("draw_set_valign",{RValue(0)});
 for(int owner=1;owner<=2;++owner){double y=panel_y-54*Global(0x1b02718);
  p.FittedText(panel_x[owner],y,owner==1?"P1":"P2",0xffffff,1,width*.2);
  p.Call("draw_set_color",{RValue(ndship::hud::PlayerColor(owner))});p.Call("draw_set_alpha",{RValue(stats_owner==owner?.9:.35)});
  p.Call("draw_line",{RValue(panel_x[owner]),RValue(y+29*Global(0x1b02718)),RValue(panel_x[owner]+120*Global(0x1b02718)),RValue(y+29*Global(0x1b02718))});
 }
 p.Call("draw_set_color",{color});p.Call("draw_set_alpha",{alpha});p.Call("draw_set_font",{font});p.Call("draw_set_halign",{align});p.Call("draw_set_valign",{valign});
}
bool Install(){
 return Hook("Pause.Builds",0x460a00,(void*)Pause,(void**)&original_pause)&&
  Hook("Pause.BuildHover",0x3f4aa0,(void*)Hover,(void**)&original_hover)&&
  Hook("Pause.Stats",0x1d7d40,(void*)Stats,(void**)&original_draw_pause);
}
}
