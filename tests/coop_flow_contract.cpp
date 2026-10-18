#include "../src/ship_coop_flow.hpp"
#include <cassert>
#include <iostream>
int main(){
 using namespace ndship::flow;
 for(double dt:{1.0/30,1.0/60,1.0/144}){
  Zoom z;z.Target(1.2);double previous=1;
  for(int i=0;i<200;++i){double next=z.Advance(dt);assert(next>=previous&&next<=1.2);previous=next;}
  assert(std::abs(z.Value()-1.2)<1e-9);z.Target(1);assert(z.Value()==1.2);
  for(int i=0;i<200;++i){double next=z.Advance(dt);assert(next<=previous&&next>=1);previous=next;}
  assert(z.Value()==1);
 }
 Zoom z;z.Target(1.2);z.Advance(.3);double current=z.Value();z.Target(1);assert(z.Value()==current);z.Advance(.1);assert(z.Value()<current);
 Rescue r;r.Fall(10,20);assert(!r.Step(4.9,true,true)&&r.progress==0);assert(!r.Step(.2,true,true)&&r.progress<.101);
 double before=r.progress;assert(!r.Step(1,true,false)&&r.progress==before);
 assert(!r.Step(1,true,true));before=r.progress;assert(!r.Step(.5,false,true)&&r.progress<before);
 assert(r.Step(4,true,true));r.Reset();assert(!r.down&&!r.Step(100,true,true));
 std::cout<<"PASS zoom continuity/settling across frame rates, rescue cooldown/proximity/decay and teammate gate\n";
}
