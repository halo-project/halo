
#include "halo/KnobSet.h"

#include "halo/nlohmann/json.hpp"

namespace halo {

KnobSet::KnobSet(const KnobSet& Other) {
  for (auto const& Entry : Other) {
    Knob* Ptr = Entry.second.get();

    if (IntKnob* K = llvm::dyn_cast<IntKnob>(Ptr))
      insert(std::make_unique<IntKnob>(*K));

    else if (FlagKnob* K = llvm::dyn_cast<FlagKnob>(Ptr))
      insert(std::make_unique<FlagKnob>(*K));

    else if (LoopKnob* K = llvm::dyn_cast<LoopKnob>(Ptr))
      insert(std::make_unique<LoopKnob>(*K));

    else
      llvm::report_fatal_error("non-exhaustive match failure");
  }
}

size_t KnobSet::size() const {
   size_t numVals = 0;

   for (auto const& Entry : Knobs)
     numVals += Entry.second->size();

   return numVals;
 }

 void KnobSet::InitializeKnobs(JSON const& Config, KnobSet& Knobs) {
   llvm::report_fatal_error("todo: read the JSON object");
 }

} // end namespace halo
