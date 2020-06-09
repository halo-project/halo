#include "halo/tuner/NamedKnobs.h"

namespace halo {
  namespace named_knob {
    std::string forLoop(unsigned i, ty NamedKnob) {
      return forLoop(i, NamedKnob.first);
    }

    std::string forLoop(unsigned i, std::string const& NamedKnob) {
      return ("loop" + std::to_string(i) + "-" + NamedKnob);
    }
  }
}

