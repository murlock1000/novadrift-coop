#pragma once
// Static ABI reconstruction for NovaDrift.exe SHA256 6555f9f49c9b44f2a38dafc0846dd8af0e7e1f1e100cba976db60967b7173e2d.
// This header records candidates; runtime validation remains required.
#include <cstdint>
#include <cmath>
namespace novadrift_native {
struct RValue { union { double real; void* pointer; std::int64_t integer; }; std::uint32_t flags; std::uint32_t kind; };
static_assert(sizeof(RValue)==16);
using Instance=void;
using Event=void(*)(Instance*,Instance*);
using Script=RValue*(*)(Instance*,Instance*,RValue*,int,RValue**);
using GetValue=bool(*)(Instance*,int,std::int32_t,RValue*,bool,bool);
using SetValue=bool(*)(Instance*,int,std::int32_t,RValue*);
using GetSlot=RValue*(*)(Instance*,int);
constexpr std::int32_t Scalar=INT32_MIN;
struct Fields {
 static constexpr std::uintptr_t X=0x1af74e8,Y=0x1af7508,Id=0x1af74d8,Angle=0x1af7528,
 HSpeed=0x1af75a8,VSpeed=0x1af75b8,Speed=0x1af7598,Target=0x1af78b8,TargetDir=0x1af7848,
 CanFire=0x1b04978,FireRegular=0x1af62f8,FireIntegrated=0x1af6288,CanFireIntegrated=0x1af7918,
 MoveSpeed=0x1af7888,RotSpeed=0x1af77f8,Hull=0x1af78d8,Alarm=0x1af76f8;
};
struct Access {
 std::uintptr_t base;
 int id(std::uintptr_t cell)const {return *reinterpret_cast<const int*>(base+cell);}
 bool get(Instance* instance,std::uintptr_t cell,double& out,std::int32_t index=Scalar)const {
  RValue v{};v.kind=5;
  if(!reinterpret_cast<GetValue>(base+0xf16590)(instance,id(cell),index,&v,false,false))return false;
  auto k=v.kind&0xffffff;
  if(k==0||k==13){out=v.real;return std::isfinite(out);}
  if(k==7){out=static_cast<std::int32_t>(v.integer);return true;}
  if(k==10){out=static_cast<double>(v.integer);return true;}
  return false;
 }
 bool set(Instance* instance,std::uintptr_t cell,double value,std::int32_t index=Scalar)const {
  RValue v{};v.real=value;
  return reinterpret_cast<SetValue>(base+0xf17460)(instance,id(cell),index,&v);
 }
};
} // namespace novadrift_native
