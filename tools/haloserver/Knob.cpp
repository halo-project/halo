#include "halo/Knob.h"


namespace halo {
  std::atomic<KnobTicket> KnobTicker {1};

template <typename S>
bool operator== (const ScalarKnob<S>& A, const ScalarKnob<S>& B) {
  return (
    A.max() == B.max() &&
    A.min() == B.min() &&
    A.getDefault() == B.getDefault()
  );
}

}
