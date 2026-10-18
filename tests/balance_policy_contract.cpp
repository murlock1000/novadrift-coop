#include "../src/ship_balance.hpp"
#include <cassert>
#include <iostream>
#include <limits>
int main(){
 using namespace ndship::balance;
 Profile p;std::string error;
 assert(Parse(Defaults(),p,error));assert(p.enabled&&p.arena_scale==1.20&&p.enemy_budget==1.60);
 assert(p.arena_scale*p.arena_scale>1.43&&p.arena_scale*p.arena_scale<1.45);
 for(const auto* text:{"arena_scale=0.2","enemy_budget=200","xp_per_enemy=nan","arena_scale=inf","enemy_budget=1.6junk","enabled=2","arena_sclae=1.2","enemy_budget 1.5 extra"}){
  auto before=p;assert(!Parse(text,p,error));assert(p.enemy_budget==before.enemy_budget&&p.arena_scale==before.arena_scale);
 }
 assert(Parse("[balance]\r\nenabled=0 ; disable\r\narena_scale=1.1\r\n",p,error)&&!p.enabled&&p.arena_scale==1.1);
 // Wrap-aware targeting and hysteresis, including either survivor and no ships.
 assert(DistanceSquared(5,10,995,10,1000,800)==100);
 assert(Target(100,90,true,true,1)==1);assert(Target(100,60,true,true,1)==2);
 assert(Target(90,100,true,true,2)==2);assert(Target(60,100,true,true,2)==1);
 assert(Target(1,900,false,true,1)==2);assert(Target(900,1,true,false,2)==1);
 assert(Target(1,1,false,false)==0);
 std::cout<<"PASS co-op policy: config rejection, wrap targets, hysteresis and survivors\n";
}
