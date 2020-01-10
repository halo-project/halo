#pragma once

#include "halo/Knob.h"
#include "halo/LoopKnob.h"

#include <unordered_map>

namespace halo {

  // using std::variant & std::visit to try and combine the
  // different templated instances of Knob<T> into a single container
  // would be nice but requires C++17, and possibly RTTI
  //
  // https://en.cppreference.com/w/cpp/utility/variant/visit
  //
  // instead, we use abstract function-objects to implement the equivalent
  // of a lambda-case in a functional language, e.g., (\x -> case x of ...)
  // and write our own generic operations over them.


  // NOTE if you add another structure member, you must immediately update:
  //
  // 1. class KnobConfig
  // 2. KnobSetAppFn and related abstract visitors.
  // 3. applyToKnobs and related generic operations.

  class KnobSet {
  public:
    std::unordered_map<KnobID, knob_type::ScalarInt*> IntKnobs;
    std::unordered_map<KnobID, knob_type::Loop*> LoopKnobs;

    size_t size() const {
      size_t numVals = 0;

      for (auto const& Entry : IntKnobs)
        numVals += Entry.second->size();

      for (auto const& Entry : LoopKnobs)
        numVals += Entry.second->size();

      return numVals;
    }

  };

  // applies some arbitrary operation to a KnobSet
  class KnobSetAppFn {
  public:
      virtual void operator()(std::pair<KnobID, knob_type::ScalarInt*>) = 0;
      virtual void operator()(std::pair<KnobID, knob_type::Loop*>) = 0;
  };

  // apply an operation over the IDs of a collection of knobs
  class KnobIDAppFn {
  public:
      virtual void operator()(KnobID) = 0;
  };

 void applyToKnobs(KnobSetAppFn &F, KnobSet const &KS);
 void applyToKnobs(KnobIDAppFn &F, KnobSet const &KS);

}
