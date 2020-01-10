
#include "halo/KnobSet.h"

namespace halo {


void applyToKnobs(KnobSetAppFn &F, KnobSet const &KS) {
   for (auto V : KS.IntKnobs) F(V);
   for (auto V : KS.LoopKnobs) F(V);
}

void applyToKnobs(KnobIDAppFn &F, KnobSet const &KS) {
   for (auto V : KS.IntKnobs) F(V.first);
   for (auto V : KS.LoopKnobs) F(V.first);
}

}
