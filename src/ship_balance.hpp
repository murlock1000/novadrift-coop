#pragma once
// Co-op tuning policy; native hooks live in coop_ship.cpp. No engine state here.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <string>

namespace ndship::balance {
inline constexpr const wchar_t* FileName=L"coop-balance.ini";
struct Profile {
    bool enabled=true;
    double arena_scale=1.20;
    double enemy_budget=1.60;
    double xp_per_enemy=0.70;
};
inline std::string Defaults() {
    return "; Co-op balance. Restart the game after editing.\n"
           "; Arena scale applies to width and height (1.20 gives 44% more area).\n"
           "; Enemy budget preserves native boss/order rules.\n"
           "; XP stays shared; each living player gets their own native choice.\n"
           "enabled=1\narena_scale=1.20\nenemy_budget=1.60\n"
           "xp_per_enemy=0.70\n";
}
inline bool Parse(const std::string& text,Profile& output,std::string& error) {
    Profile candidate;std::istringstream lines(text);std::string line;
    while(std::getline(lines,line)) {
        const auto comment=line.find_first_of(";#");if(comment!=std::string::npos)line.resize(comment);
        std::replace(line.begin(),line.end(),'=',' ');
        std::istringstream row(line);std::string key,value,extra;
        if(!(row>>key))continue;
        if(key.front()=='['&&key.back()==']')continue;
        if(!(row>>value)||(row>>extra)){error="Invalid co-op balance setting: "+key;return false;}
        char* end=nullptr;const double number=std::strtod(value.c_str(),&end);
        if(!end||*end||!std::isfinite(number)){error="Invalid co-op balance number: "+key;return false;}
        double* field=nullptr;double minimum=1,maximum=2;
        if(key=="enabled") {
            if(number!=0&&number!=1){error="Co-op balance enabled must be 0 or 1";return false;}
            candidate.enabled=number!=0;continue;
        }
        if(key=="arena_scale"){field=&candidate.arena_scale;maximum=1.5;}
        else if(key=="enemy_budget"){field=&candidate.enemy_budget;maximum=2.5;}
        else if(key=="xp_per_enemy"){field=&candidate.xp_per_enemy;minimum=0.25;maximum=1.5;}
        else {error="Unknown co-op balance setting: "+key;return false;}
        if(number<minimum||number>maximum){error="Co-op balance setting outside supported range: "+key;return false;}
        *field=number;
    }
    output=candidate;error.clear();return true;
}
// Torus distance: ships close across a wrap edge count as neighbors.
inline double AxisDistance(double a,double b,double extent) {
    double distance=std::abs(a-b);
    if(std::isfinite(extent)&&extent>0){distance=std::fmod(distance,extent);distance=std::min(distance,extent-distance);}
    return distance;
}
inline double DistanceSquared(double x,double y,double target_x,double target_y,double width,double height) {
    const double dx=AxisDistance(x,target_x,width),dy=AxisDistance(y,target_y,height);return dx*dx+dy*dy;
}
// A 20% distance advantage is required to switch an established living target.
inline int Target(double distance1,double distance2,bool alive1,bool alive2,int previous=0) {
    if(!alive1)return alive2?2:0;
    if(!alive2)return 1;
    if(previous==1&&distance2>=distance1*0.64)return 1;
    if(previous==2&&distance1>=distance2*0.64)return 2;
    return distance2<distance1?2:1;
}
}
