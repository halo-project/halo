#include "halo/tuner/RandomTuner.h"
#include "halo/tuner/KnobSet.h"
#include "halo/tuner/Knob.h"

#include "Logging.h"

namespace halo {
namespace RandomTuner {

template < typename RNE >  // meets the requirements of RandomNumberEngine
KnobSet randomFrom(KnobSet const& KS, RNE &Eng) {
  KnobSet New(KS);

  for (auto &Entry : New) {
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
      fatal_error("randomFrom -- unimplemented knob kind encountered");
    }
  }
  return New;
}



// generates a random integer that is "nearby" an
// existing integer, within the given inclusive range
// [min, max], given the amount of energy we have
// to move away from the current integer.
// Energy must be within [0, 100].
//
// NOTE: the returned integer may be equal to the existing one.
template < typename RNE >
int nearbyInt (RNE &Eng, int cur, int min, int max, double energy) {
  assert(min <= cur && cur <= max && "invalid ranges in nearbyInt");
  assert(0 <= energy && energy <= 100 && "invalid energy level");

  // 68% of values drawn will be within this distance from the old value.
  int range = std::abs(max - min);
  int scaledRange = range * (energy / 100.0);
  int stdDev = scaledRange / 2.0;

  // sample from a normal distribution, where the mean is
  // the old value, and the std deviation is influenced by the energy.
  // NOTE: a logistic distribution, which is like a higher kurtosis
  // normal distribution, might give us better numbers when the
  // energy is low?
  std::normal_distribution<double> dist(cur, stdDev);

  // ensure the value is in the right range.
  int val = std::round(dist(Eng));
  val = std::max(val, min);
  val = std::min(val, max);

  return val;
}



template < typename RNE >
KnobSet nearby(KnobSet const& KS, RNE &Eng, float energy) {
  KnobSet New(KS);

  for (auto &Entry : New) {
    auto Ptr = Entry.second.get();

    if (IntKnob *IK = llvm::dyn_cast<IntKnob>(Ptr)) {
      // NOTE: not using scaled val, since the scaled space is sparse and doesn't make sense to pick values from!
      IK->setVal(nearbyInt<RNE>(Eng, IK->getVal(), IK->getMin(), IK->getMax(), energy));

    } else if (FlagKnob *FK = llvm::dyn_cast<FlagKnob>(Ptr)) {
      FK->setVal(nearbyInt<RNE>(Eng, FK->getVal(), FK->getMin(), FK->getMax(), energy));

    } else if (OptLvlKnob *OK = llvm::dyn_cast<OptLvlKnob>(Ptr)) {
      unsigned Min = OptLvlKnob::asInt(OK->getMin());
      unsigned Max = OptLvlKnob::asInt(OK->getMax());
      unsigned Cur = OptLvlKnob::asInt(OK->getVal());

      unsigned NearbyLevel = nearbyInt<RNE>(Eng, Cur, Min, Max, energy);

      OK->setVal(OptLvlKnob::parseLevel(NearbyLevel));

    } else {
      fatal_error("nearby -- unimplemented knob kind encountered");
    }
  }
  return New;
}


// specializations
template KnobSet randomFrom<std::mt19937_64>(KnobSet const&, std::mt19937_64&);
template KnobSet nearby<std::mt19937_64>(KnobSet const& KS, std::mt19937_64 &Eng, float energy);



} // end namespace RandomTuner
} // end namespace halo