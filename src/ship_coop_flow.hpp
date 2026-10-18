#pragma once
#include <algorithm>
#include <cmath>

namespace ndship::flow {
class Zoom {
    double from_=1,value_=1,target_=1,elapsed_=0;
public:
    static constexpr double Duration=0.85;
    void Reset(){from_=value_=target_=1;elapsed_=0;}
    void Target(double value){if(std::abs(value-target_)<1e-8)return;from_=value_;target_=value;elapsed_=0;}
    double Advance(double seconds){elapsed_=std::min(Duration,elapsed_+std::max(0.0,seconds));double t=elapsed_/Duration;t=t*t*(3-2*t);value_=from_+(target_-from_)*t;return value_;}
    double Value()const{return value_;}
    double TargetValue()const{return target_;}
};
struct Rescue {
    static constexpr double Delay=5,Duration=3,Radius=145;
    bool down=false;
    double x=0,y=0,age=0,progress=0;
    void Fall(double px,double py){down=true;x=px;y=py;age=progress=0;}
    void Reset(){*this={};}
    bool Step(double seconds,bool in_range,bool partner_living){
        if(!down||!partner_living)return false;
        const double previous_age=age;age+=std::max(0.0,seconds);
        if(age<Delay)return false;
        const double available=std::min(std::max(0.0,seconds),age-std::max(previous_age,Delay));
        progress=std::clamp(progress+(in_range?available:-available*.7),0.0,Duration);
        return progress>=Duration;
    }
    double Fraction()const{return progress/Duration;}
};
}
