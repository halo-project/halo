
#include "halo/KnobSet.h"

namespace halo {

size_t KnobSet::size() const {
   size_t numVals = 0;

   for (auto const& Entry : Knobs)
     numVals += Entry.second->size();

   return numVals;
 }

} // end namespace halo
