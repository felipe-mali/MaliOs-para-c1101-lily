#include "../../src/core/ui/MaliMotion.h"
using namespace MaliUI;
static_assert(wrap(6,6)==0 && wrap(-1,6)==5,"circular menu endpoints");
static_assert(wrap(1000000001LL,6)==5 && wrap(-1000000001LL,6)==1,"large batches preserve exact index");
constexpr bool animation(){GearMotion m;m.move(1,100);if(m.offset!=1024||!m.active)return false;m.tick(160);if(m.offset<=0||m.offset>=1024)return false;m.tick(220);return m.offset==0&&!m.active&&m.phase==-300;}
static_assert(animation(),"120ms eased slide completes and stops");
constexpr bool reverse(){GearMotion m;m.move(1,0);m.tick(50);int phase=m.phase;m.move(-1,50);if(m.phase!=wrap(phase,3600)||m.duration!=90)return false;m.tick(140);return !m.active&&m.offset==0&&m.phase>phase;}
static_assert(reverse(),"reverse during transition, no queued animation");
constexpr bool rapid(){GearMotion m;int index=0;for(int i=0;i<1000;++i){int step=i%3?3:-2;index=wrap(index+step,6);m.move(step,i);if(m.offset>1024||m.offset<-1024)return false;}m.tick(1200);return !m.active&&index==wrap(666*3-334*2,6);}
static_assert(rapid(),"rapid detents update index without unbounded animation backlog");
constexpr bool rollover(){GearMotion m;m.move(1,0xfffffff0);m.tick(0x80);return !m.active&&m.offset==0;}
static_assert(rollover(),"millis rollover");
constexpr bool button(){HoldButton b;if(b.update(true,0)!=ButtonEvent::None)return false;b.update(false,10);b.update(true,20);if(b.update(false,100)!=ButtonEvent::Select)return false;b.update(true,200);if(b.update(true,850)!=ButtonEvent::Back)return false;return b.update(false,860)==ButtonEvent::None;}
static_assert(button(),"held-at-entry is ignored, click selects, long hold backs once");
constexpr bool entryHold(){HoldButton b;b.update(true,0);b.update(true,1000);return !b.armed&&b.update(false,1100)==ButtonEvent::None;}
static_assert(entryHold(),"enter/exit cannot activate next menu with inherited hold");
