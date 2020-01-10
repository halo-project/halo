#pragma once

#include <cinttypes>
#include <climits>
#include <string>
#include <cassert>
#include <atomic>

#include "halo/Utility.h"

namespace halo {

  // polymorphism of Knob values is primarily achieved through template
  // type params.
  // inheritance via LLVM-style RTTI is used for sub-typing:
  //
  // https://www.llvm.org/docs/HowToSetUpLLVMStyleRTTI.html
  //
  // Thus, you should use isa<> and dyn_cast<> for down-casts.

  // I really wish c++ supported template methods that are virtual!
  // We implement the template portion manually with macros.
  // If you see things like "HANDLE_CASE" that take a type as a parameter,
  // that's what we mean. This is still robust, since a new virtual method will
  // cause the compiler to point out places where you haven't updated your code

  using KnobTicket = uint64_t;

  // used to ensure knob IDs are unique.
  // we rely on the fact that 0 is an invalid knob ID
  extern std::atomic<KnobTicket> KnobTicker;

  // Base class for tunable compiler "knobs", which
  // are simply tunable components.
  template< typename ValTy >
  class Knob {

  private:
    KnobTicket ticker__;

  public:
    Knob() {
      ticker__ = KnobTicker.fetch_add(1);
      assert(ticker__ != 0 && "exhausted the knob ticker!");
    }
    virtual ~Knob() = default;
    // value accessors
    virtual ValTy getDefault() const = 0;
    virtual ValTy getVal() const = 0;
    virtual void setVal(ValTy) = 0;
    // a human-readable but unique name that idenifies this tunable knob
    virtual std::string const& getID() const = 0;

    // a unique number relative to all knobs in the current process only.
    // suitable for use in the ID of a dynamically generated Knob
    KnobTicket getTicket() const { return ticker__; }

    // members related to exporting to a flat array

    virtual size_t size() const { return 1; } // num values to be flattened

  }; // end class Knob


  // represents a knob that can take on values in the range
  // [a, b], where a, b are scalar values.
  template < typename ValTy >
  class ScalarKnob : public Knob<ValTy> {
  public:
    virtual ~ScalarKnob() = default;

    // inclusive ranges
    virtual ValTy min() const = 0;
    virtual ValTy max() const = 0;

    template <typename S>
    friend bool operator== (ScalarKnob<S> const&, ScalarKnob<S> const&);

  }; // end class ScalarKnob


  // a boolean-like scalar range
  class FlagKnob : public ScalarKnob<int> {
  private:
    static constexpr int TRUE = 1;
    static constexpr int FALSE = 0;
    int current;
    int dflt;
  public:
    virtual ~FlagKnob() = default;
    FlagKnob(bool dflt_) : dflt(dflt_ ? TRUE : FALSE) {
      current = dflt;
    }
    int getDefault() const override { return dflt; }
    int getVal() const override { return current; }
    void setVal(int newVal) override {
      assert(newVal == TRUE || newVal == FALSE);
      current = newVal;
    }
    int min() const override { return FALSE; }
    int max() const override { return TRUE; }

    bool getFlag() const {
      return current != FALSE;
    }

  }; // end class FlagKnob


} // namespace tuner
