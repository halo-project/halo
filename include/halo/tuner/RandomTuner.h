#include <random>

namespace halo {

class KnobSet;

// There is no "random tuner" type. It's just a collection
// of utilities for randomly changing a knob set.
namespace RandomTuner {

  // Generates a KnobSet that is a completely random fabrication
  // based on the given KnobSet.
  //
  // By "completely random" I mean that is is blind to
  // the existing values of the provided knobs, it just picks some values
  // out of "thin-air" for every knob in the returned set.
  // It does ensure that the sets have the same size, etc.
  //
  // RNE meets the requirements of RandomNumberEngine
  template < typename RNE >
  KnobSet randomFrom(KnobSet const& KS, RNE &Eng);


  // Generates a KnobSet that is "nearby" the given KnobSet's settings.
  //
  // Energy must be within [0, 100]. The higher the value, the higher the
  // probability that each knob will be set to a value that is further from
  // its value in the input set.
  //
  // You can think of the input KnobSet as a particle vibrating in a size()-dimensional
  // space, and the energy-level determines the neighborhood around the particle that
  // it's vibrating in. The nearby positions are places where the KnobSet returned will
  // be.
  //
  // RNE meets the requirements of RandomNumberEngine
  template < typename RNE >
  KnobSet nearby(KnobSet const& KS, RNE &Eng, float energy);


  // specializations
  extern template KnobSet randomFrom<std::mt19937_64>(KnobSet const&, std::mt19937_64&);
  extern template KnobSet nearby<std::mt19937_64>(KnobSet const& KS, std::mt19937_64 &Eng, float energy);

} // end namespace RandomTuner

} // end namespace halo