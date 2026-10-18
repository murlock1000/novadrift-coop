#pragma once
// Explicit, bounded native stimuli. Called only by the existing QA command gate.
namespace CoopBalance {
void Probe(int mode,int counter){
 if(!qa_mode||!trace||!Active())return;
 auto first=Resolve(ships[1]),second=Resolve(ships[2]);
 const double left=Global(0x1af7428),top=Global(0x1af74a8),width=Global(0x1af7468),height=Global(0x1af7418),cx=left+width/2,cy=top+height/2;
 if(!(width>0&&height>0))return;
 struct Saved{void* ship;double x,y,h,v;};
 Saved saved[2]={{first,Read(first,nd::Fields::X),Read(first,nd::Fields::Y),Read(first,nd::Fields::HSpeed),Read(first,nd::Fields::VSpeed)},
                 {second,Read(second,nd::Fields::X),Read(second,nd::Fields::Y),Read(second,nd::Fields::HSpeed),Read(second,nd::Fields::VSpeed)}};
 const double cloak1=Read(Resolve(powers[1]),0x1afbb78,0),cloak2=Read(Resolve(powers[2]),0x1afbb78,0);
 RValue visible1,visible2;if(first)api->GetBuiltin("visible",(CInstance*)first,NULL_INDEX,visible1);if(second)api->GetBuiltin("visible",(CInstance*)second,NULL_INDEX,visible2);
 RValue visible(true);if(first)api->SetBuiltin("visible",(CInstance*)first,NULL_INDEX,visible);if(second)api->SetBuiltin("visible",(CInstance*)second,NULL_INDEX,visible);
 auto position=[&](void* ship,double x,double y){if(ship){access.set(ship,nd::Fields::X,x);access.set(ship,nd::Fields::Y,y);access.set(ship,nd::Fields::HSpeed,0);access.set(ship,nd::Fields::VSpeed,0);}};
 for(auto& old:saved)position(old.ship,old.ship==first?cx-width*.25:cx+width*.20,cy);
 if(first)access.set(Resolve(powers[1]),0x1afbb78,0);if(second)access.set(Resolve(powers[2]),0x1afbb78,0);
 ndship::OwnerScope world(0);ndship::TargetScope no_target(0);
 auto made=Builtin("instance_create_depth",{RValue(cx),RValue(cy),RValue(0.0),RValue(552)});int enemy_id=(int)made.m_i64;auto enemy=Resolve(enemy_id);
 if(!enemy){Log("QA balance could not create native Interceptor");return;}
 auto choose=[&](const char* phase,double x){
  position(enemy,x,cy);nd::RValue result{};result.kind=5;
  ((nd::Script)(access.base+0xe7a60))(enemy,enemy,&result,0,nullptr);
  auto target=Field(enemy,nd::Fields::Target);const int id=target.m_Kind==VALUE_REF?(int)target.m_i64:target.ToInt32();
  std::fprintf(trace,"{\"kind\":\"target_probe\",\"counter\":%d,\"phase\":\"%s\",\"target\":%d,\"target_kind\":%d,\"target_payload\":%llu,\"p1\":%d,\"p2\":%d,\"target_x\":%.8f,\"target_y\":%.8f,\"x1\":%.8f,\"x2\":%.8f,\"width\":%.8f,\"owner\":%d}\n",counter,phase,id,target.m_Kind,(unsigned long long)target.m_i64,ships[1],ships[2],Read(enemy,0x1aff038),Read(enemy,0x1aff018),Read(first,nd::Fields::X),Read(second,nd::Fields::X),width,Owner(enemy));
 };
 if(mode==1&&first&&second){
  choose("p2_close",cx+width*.20-30);choose("p1_close",cx-width*.25+30);
  position(first,left+width*.4,cy);position(second,left+width-10,cy);choose("wrap_p2",left+5);
  position(first,cx-width*.25,cy);position(second,cx+width*.20,cy);
  access.set(Resolve(powers[2]),0x1afbb78,1);choose("p2_cloaked",cx+width*.20-30);access.set(Resolve(powers[2]),0x1afbb78,0);
  // The added margin must be genuinely playable before the native wrap fires.
  const double inside_x=left+width*.03;position(second,inside_x,cy);nd::RValue ignored{};ignored.kind=5;
  ((nd::Script)(access.base+0x796f00))(second,second,&ignored,0,nullptr);
  std::fprintf(trace,"{\"kind\":\"arena_probe\",\"counter\":%d,\"native_width\":%.8f,\"native_height\":%.8f,\"width\":%.8f,\"height\":%.8f,\"inside_x\":%.8f,\"after_wrap_x\":%.8f,\"left\":%.8f,\"camera_updates\":%llu}\n",counter,last_native_width,last_native_height,width,height,inside_x,Read(second,nd::Fields::X),left,arena_updates);
  // Exercise the actual pickup award routine with normal and scripted pickups.
  for(int asset:{284,285,286,289}){
   auto pickup=Builtin("instance_create_depth",{RValue(cx),RValue(cy),RValue(0.0),RValue(asset)});auto p=Resolve((int)pickup.m_i64);
   if(p){nd::RValue args[4]{};args[0].real=1;args[1].real=0;args[2].kind=args[3].kind=13;nd::RValue* argv[]={args,args+1,args+2,args+3};nd::RValue result{};result.kind=5;auto c=Resolve(controller_id);double before=Read(c,0x1a9da98,0);
    ((nd::Script)(access.base+0x781240))(p,second,&result,4,argv);
    std::fprintf(trace,"{\"kind\":\"xp_probe\",\"counter\":%d,\"asset\":%d,\"delta\":%.8f,\"native_multiplier\":%.8f}\n",counter,asset,Read(c,0x1a9da98,0)-before,Global(0x1affeb8)*Global(0x1aee5c8));
    Builtin("instance_destroy",{pickup});
   }
  }
  auto waves=Builtin("instance_find",{RValue(862),RValue(0)});auto w=Resolve((int)waves.m_i64);
  if(!w){waves=Builtin("instance_create_depth",{RValue(0.0),RValue(0.0),RValue(0.0),RValue(862)});w=Resolve((int)waves.m_i64);}
  if(w){auto original_wave=Builtin("variable_global_get",{RValue("wave")});
   for(int wave=1;wave<=12;++wave){Builtin("variable_global_set",{RValue("wave"),RValue(wave)});nd::RValue ignored{};ignored.kind=5;((nd::Script)(access.base+0x61b170))(w,w,&ignored,0,nullptr);((nd::Script)(access.base+0x62df50))(w,w,&ignored,0,nullptr);}
   Builtin("variable_global_set",{RValue("wave"),original_wave});
  }
 } else choose("survivor",cx);
 Builtin("instance_destroy",{Ref(enemy_id)});
 for(auto& old:saved)if(old.ship){position(old.ship,old.x,old.y);access.set(old.ship,nd::Fields::HSpeed,old.h);access.set(old.ship,nd::Fields::VSpeed,old.v);}
 if(first)access.set(Resolve(powers[1]),0x1afbb78,cloak1);if(second)access.set(Resolve(powers[2]),0x1afbb78,cloak2);
 if(first)api->SetBuiltin("visible",(CInstance*)first,NULL_INDEX,visible1);if(second)api->SetBuiltin("visible",(CInstance*)second,NULL_INDEX,visible2);
 std::fprintf(trace,"{\"kind\":\"probe_done\",\"counter\":%d,\"mode\":%d}\n",counter,mode);std::fflush(trace);
}
}
