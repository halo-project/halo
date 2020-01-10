#pragma once

#include <cinttypes>
#include <climits>
#include <string>
#include <cassert>
#include <atomic>

#include <halo/Utility.h>

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

  using KnobID = uint64_t;

  // used to ensure knob IDs are unique.
  // we rely on the fact that 0 is an invalid knob ID
  extern std::atomic<KnobID> KnobTicker;

  // Base class for tunable compiler "knobs", which
  // are simply tunable components.
  template< typename ValTy >
  class Knob {

  private:
    KnobID id__;

  public:
    Knob() {
      id__ = KnobTicker.fetch_add(1);
      assert(id__ != 0 && "exhausted the knob ticker!");
    }
    virtual ~Knob() = default;
    // value accessors
    virtual ValTy getDefault() const = 0;
    virtual ValTy getVal() const = 0;
    virtual void setVal(ValTy) = 0;

    // a unique ID relative to all knobs in the process.
    // since multiple instances of autotuners can be created
    // per process, this only guarentees uniqueness of each
    // instance, it is otherwise unstable.
    KnobID getID() const { return id__; }

    virtual std::string getName() const {
       return "knob id " + std::to_string(getID());
    }

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

////////////////////////
// handy type aliases and type utilities

namespace knob_type {
  using ScalarInt = ScalarKnob<int>;
}

// this needs to appear first, before specializations.
template< typename Any >
struct is_knob {
  static constexpr bool value = false;
  using rawTy = void;
};


} // namespace tuner
