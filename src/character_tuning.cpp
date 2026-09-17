#include "character_tuning.h"
#include "guarded_patch.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace cv4tuning {
namespace {
struct Patch {
  GuardedPatch guard{};
  Parameter parameter;
  int original, sign, shift;
  bool percent;
};
// Stable storage: registered patch addresses must not move.
Patch patches[48];
unsigned count;
bool ready;
uint8_t *image;
unsigned imageSize;

void add(unsigned offset, uint8_t opcode, int original, Parameter parameter,
         bool percent=false, int sign=1, int shift=0) {
  if(count>=48 || offset+3>imageSize) { ready=false; return; }
  Patch &p=patches[count++];
  p.parameter=parameter; p.original=original; p.percent=percent;
  p.sign=sign; p.shift=shift;
  uint8_t expected[3]={opcode, uint8_t(original), uint8_t(original>>8)};
  // Data words use opcode=0 and point directly to the word.
  const uint8_t *bytes=opcode ? expected : expected+1;
  size_t size=opcode ? 3 : 2;
  if(std::memcmp(image+offset, bytes, size)!=0) {
    std::fprintf(stderr,"[character_tuning] Byte mismatch at ROM offset %06x (%s)\n",offset,definitions[parameter].key);
    ready=false; return;
  }
  guarded_patch_init(&p.guard, definitions[parameter].label,
                     image+offset,size,bytes,bytes);
  if(guarded_patch_register(&p.guard)!=kGuardedPatch_Ok) ready=false;
}
void velocity(unsigned offset, int32_t original, Parameter parameter) {
  // Two LDA #word / STA absolute pairs in the native player-only routines.
  add(offset,0xa9,original&0xffff,parameter,true,original<0?-1:1,0);
  add(offset+6,0xa9,(uint32_t(original)>>16)&0xffff,parameter,true,original<0?-1:1,16);
}
int fixedBase(Parameter p) {
  switch(p) {
    case Walk: case AirSpeed: return 0x14000;
    case Crouch: return 0x6000;
    case Jump: return 0x48000;
    case Gravity: return 0x6000;
    case AirControl: return 0x4000;
    default: return 0;
  }
}
}
void init(const uint8_t *rom,unsigned size) {
  for(unsigned i=0;i<count;i++) {
    guarded_patch_revert(&patches[i].guard);
    guarded_patch_unregister(&patches[i].guard);
  }
  count=0; ready=true; image=const_cast<uint8_t*>(rom); imageSize=size;
  // LoROM offsets. Each opcode/operand is verified against the supported USA ROM.
  velocity(0x265e,0x14000,Walk); velocity(0x2677,-0x14000,Walk);
  velocity(0x26fe,0x6000,Crouch); velocity(0x270f,-0x6000,Crouch);
  velocity(0x2398,-0x48000,Jump);
  velocity(0x23ab,0x14000,AirSpeed); velocity(0x23be,-0x14000,AirSpeed);
  add(0x273d,0x69,0x6000,Gravity,true);
  for(auto op : {0x2758,0x2763}) add(op,op==0x2758?0xe9:0xa9,8,FallSpeed);
  // Air speed is clamped in both directions, including the fractional word.
  add(0x2904,0xe9,0x4000,AirSpeed,true); add(0x290a,0xe9,1,AirSpeed,true,1,16);
  add(0x290f,0xa9,1,AirSpeed,true,1,16); add(0x2915,0xa9,0x4000,AirSpeed,true);
  add(0x2935,0xe9,0xc000,AirSpeed,true,-1); add(0x293b,0xe9,0xfffe,AirSpeed,true,-1,16);
  add(0x2940,0xa9,0xfffe,AirSpeed,true,-1,16); add(0x2946,0xa9,0xc000,AirSpeed,true,-1);
  add(0x28ef,0x69,0x4000,AirControl,true); add(0x2920,0x69,0xc000,AirControl,true,-1);
  // Air-control slider is capped below one pixel/frame, so carry words stay native.
  const Parameter damage[]={LeatherDamage,LeatherLimp,ChainDamage,ChainLimp,LongDamage,LongLimp};
  for(unsigned i=0;i<6;i++) add(0xa6ec+i*2,0,definitions[damage[i]].normal,damage[i]);
  for(int i=0;i<4;i++) add(0xa6fa+i*2,0,definitions[DaggerDamage+i].normal,Parameter(DaggerDamage+i));
  for(int i=0;i<3;i++) add(0x9261+i*2,0,definitions[LeatherLength+i].normal,Parameter(LeatherLength+i));
  for(int i=0;i<5;i++) add(0x9257+i*2,0,definitions[DaggerCost+i].normal,Parameter(DaggerCost+i));
  if(!ready) std::fprintf(stderr,"[character_tuning] Unsupported bytes; gameplay tuning disabled\n");
}
bool available() { return ready; }
bool apply(const Values &values) {
  if(!ready) return false;
  for(unsigned i=0;i<count;i++) {
    Patch &p=patches[i]; const auto &d=definitions[p.parameter];
    int value=std::clamp(values.value[p.parameter],d.minimum,d.maximum);
    uint32_t result=p.percent ? uint32_t(int64_t(fixedBase(p.parameter))*value/100*p.sign) : uint32_t(value);
    result>>=p.shift;
    uint8_t *target=p.guard.replacement+(p.guard.size==3?1:0);
    if(target[0]==uint8_t(result) && target[1]==uint8_t(result>>8)) {
      if(!p.guard.applied && std::memcmp(p.guard.expected,p.guard.replacement,p.guard.size))
        if(guarded_patch_apply(&p.guard)!=kGuardedPatch_Ok) return false;
      continue;
    }
    if(p.guard.applied && guarded_patch_revert(&p.guard)!=kGuardedPatch_Ok) return false;
    target[0]=uint8_t(result); target[1]=uint8_t(result>>8);
    if(std::memcmp(p.guard.expected,p.guard.replacement,p.guard.size) &&
       guarded_patch_apply(&p.guard)!=kGuardedPatch_Ok) return false;
  }
  return true;
}
}
