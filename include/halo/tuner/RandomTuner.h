#include <random>

namespace halo {

class KnobSet;

template < typename RNE >  // meets the requirements of RandomNumberEngine
void randomlyChange(KnobSet &KS, RNE &Eng);

// specializations
extern template void randomlyChange<std::mt19937_64>(KnobSet &, std::mt19937_64&);

} // end namespace