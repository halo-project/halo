#include "halo/tuner/RandomTuner.h"
#include "halo/tuner/KnobSet.h"
#include "halo/tuner/Knob.h"

#include "Logging.h"

namespace halo {
namespace RandomTuner {

template < typename RNE >  // meets the requirements of RandomNumberEngine
KnobSet randomFrom(KnobSet &&New, RNE &Eng) {
  for (auto &Entry : New) {
    auto Ptr = Entry.second.get();

    ScalarKnob<int> *SK = nullptr;

    if (SK == nullptr)
      SK = llvm::dyn_cast<IntKnob>(Ptr);

    if (SK == nullptr)
      SK = llvm::dyn_cast<FlagKnob>(Ptr);

    if (SK) {
      const int NONE = SK->getMin()-1; // representation of llvm::None;

      std::uniform_int_distribution<int> dist(NONE, SK->getMax());

      llvm::Optional<int> NewVal;
      int RandInt = dist(Eng);

      if (RandInt != NONE)
        NewVal = RandInt;

      SK->setVal(NewVal);

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
KnobSet nearby(KnobSet &&New, RNE &Eng, float energy) {
  for (auto &Entry : New) {
    auto Ptr = Entry.second.get();

    ScalarKnob<int> *SK = nullptr;

    if (SK == nullptr)
      SK = llvm::dyn_cast<IntKnob>(Ptr);

    if (SK == nullptr)
      SK = llvm::dyn_cast<FlagKnob>(Ptr);

    if (SK) {
      // NOTE: not using scaled val, since the scaled space is sparse and doesn't make sense to pick values from!
      const int NONE = SK->getMin()-1; // representation of llvm::None;

      int CurrentVal = NONE;
      SK->applyVal(CurrentVal);

      llvm::Optional<int> NewVal;
      int Nearby = nearbyInt<RNE>(Eng, CurrentVal, NONE, SK->getMax(), energy);

      if (Nearby != NONE)
        NewVal = Nearby;

      SK->setVal(NewVal);

    } else if (OptLvlKnob *OK = llvm::dyn_cast<OptLvlKnob>(Ptr)) {
      unsigned Min = OptLvlKnob::asInt(OK->getMin());
      unsigned Max = OptLvlKnob::asInt(OK->getMax());

      assert(OK->hasVal());
      unsigned Cur;
      OK->applyVal([&](OptLvlKnob::LevelTy Lvl) {
        Cur = OptLvlKnob::asInt(Lvl);
      });

      unsigned NearbyLevel = nearbyInt<RNE>(Eng, Cur, Min, Max, energy);

      OK->setVal(OptLvlKnob::parseLevel(NearbyLevel));

    } else {
      fatal_error("nearby -- unimplemented knob kind encountered");
    }
  }
  return New;
}


// specializations
template KnobSet randomFrom<std::mt19937_64>(KnobSet &&, std::mt19937_64&);
template KnobSet nearby<std::mt19937_64>(KnobSet &&, std::mt19937_64 &Eng, float energy);



} // end namespace RandomTuner
} // end namespace halo