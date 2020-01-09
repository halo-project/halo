#pragma once

#include <halo/Knob.h>

namespace halo {

  // TODO: move these definitions out of the header

// FIXME: if this code survives until Concepts
// come out, then you'll want this ValTy to be
// constrained to be a numeric supporting ++, --, ==, etc.

// Represents a knob that can take on values in the range
// [a, b], where a, b are scalar values.
template < typename ValTy >
class ScalarKnob : public Knob<ValTy> {
private:
  std::string Name;
  ValTy Default;
  ValTy Current;
  ValTy Min;
  ValTy Max;
public:
  ScalarKnob(ValTy dflt, ValTy min, ValTy max)
    : Knob<ValTy>(Knob<ValTy>::KK_ScalarKnob),
      Default(dflt), Current(dflt), Min(min), Max(max) {}

  // for LLVM-style RTTI
  static bool classof(const Knob<ValTy> *K) {
    return K->getKind() == Knob<ValTy>::KK_ScalarKnob;
  }

  virtual ~ScalarKnob() = default;

  std::string const& getName() const override {
    return Name;
  }

  ValTy getDefault() const override {
    return Default;
  }

  ValTy getVal() const override {
    return Current;
  }

  void setVal(ValTy NewV) override {
    Current = NewV;
  }

  // inclusive ranges
  ValTy min() const { return Min; }
  ValTy max() const { return Max; }

  // template <typename S>
  // friend bool operator== (ScalarKnob<S> const&, ScalarKnob<S> const&);

}; // end class ScalarKnob

// handy type aliases
using IntKnob = ScalarKnob<int>;


} // end namespace halo
