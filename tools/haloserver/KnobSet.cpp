
#include "halo/KnobSet.h"

namespace halo {

size_t KnobSet::size() const {
   size_t numVals = 0;

   for (auto const& Entry : IntKnobs)
     numVals += Entry.second->size();

   for (auto const& Entry : LoopKnobs)
     numVals += Entry.second->size();

   return numVals;
 }

void KnobSet::applyToKnobs(KnobSetAppFn &&F) {
   for (auto &V : IntKnobs) F(*V.second);
   for (auto &V : LoopKnobs) F(*V.second);
}

} // end namespace halo
