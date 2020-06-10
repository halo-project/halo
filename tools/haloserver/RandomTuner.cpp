#include "halo/tuner/RandomTuner.h"
#include "halo/tuner/KnobSet.h"
#include "halo/tuner/Knob.h"

#include "Logging.h"

namespace halo {

template < typename RNE >  // meets the requirements of RandomNumberEngine
void randomlyChange(KnobSet &KS, RNE &Eng) {
  for (auto &Entry : KS) {
    auto Ptr = Entry.second.get();

    if (IntKnob *IK = llvm::dyn_cast<IntKnob>(Ptr)) {
      std::uniform_int_distribution<> dist(IK->getMin(), IK->getMax());
      IK->setVal(dist(Eng));

    } else if (FlagKnob *FK = llvm::dyn_cast<FlagKnob>(Ptr)) {
      std::uniform_int_distribution<> dist(FK->getMin(), FK->getMax());
      FK->setVal(dist(Eng));

    } else if (OptLvlKnob *OK = llvm::dyn_cast<OptLvlKnob>(Ptr)) {
      unsigned Min = OptLvlKnob::asInt(OK->getMin());
      unsigned Max = OptLvlKnob::asInt(OK->getMax());
      std::uniform_int_distribution<> dist(Min, Max);

      OK->setVal(OptLvlKnob::parseLevel(dist(Eng)));

    } else {
      fatal_error("randomlyChange -- unimplemented knob kind encountered");
    }
  }
}

// specializations
template void randomlyChange<std::mt19937_64>(KnobSet &, std::mt19937_64&);

} // end namespace