#pragma once

#include "halo/Knob.h"
#include "halo/LoopKnob.h"

#include <unordered_map>

namespace halo {

  // handy type aliases.
  namespace knob_type {
    using LoopKnob = LoopKnob;
    using ScalarInt = ScalarKnob<int>;
  }

  // using std::variant & std::visit to try and combine the
  // different templated instances of Knob<T> into a single container
  // would be nice but requires C++17, and possibly RTTI
  //
  // https://en.cppreference.com/w/cpp/utility/variant/visit
  //
  // instead, we use abstract function-objects + ad-hoc polymorphism to
  // implement the equivalent of a lambda-case in a functional language,
  // e.g., (\x -> case x of type1 -> ... | type2 -> ... etc)

  // applies some arbitrary operation to a KnobSet
  class KnobSetAppFn {
  public:
      virtual void operator()(knob_type::ScalarInt&) = 0;
      virtual void operator()(knob_type::LoopKnob&) = 0;
  };


  class KnobSet {
  private:
    std::unordered_map<std::string, std::unique_ptr<knob_type::ScalarInt>> IntKnobs;
    std::unordered_map<std::string, std::unique_ptr<knob_type::LoopKnob>> LoopKnobs;

  public:

    template <class... Args>
    /*ScalarInt&*/ void emplaceScalarInt(Args&&... args) {
      IntKnobs.emplace(std::forward(args)...);
    }

    size_t size() const;

    void applyToKnobs(KnobSetAppFn &&F);

  };

}
