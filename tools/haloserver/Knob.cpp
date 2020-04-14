#include "halo/tuner/Knob.h"


namespace halo {
  std::atomic<KnobTicket> KnobTicker {1};

  bool operator <= (OptLvlKnob::LevelTy const& a, OptLvlKnob::LevelTy const& b) {
    assert(!(a.isOptimizingForSize() || b.isOptimizingForSize())
      && "size optimization is not accounted for in this operator!");
    return a.getSpeedupLevel() <= b.getSpeedupLevel();
  }

}
