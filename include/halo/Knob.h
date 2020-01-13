#pragma once

#include <cinttypes>
#include <limits>
#include <string>
#include <cassert>
#include <atomic>

#include "halo/Utility.h"

#include "llvm/Support/Casting.h"

namespace halo {

  // LLVM-style RTTI is used for subtyping:
  //
  // https://www.llvm.org/docs/HowToSetUpLLVMStyleRTTI.html
  //
  // Thus, you should use isa<> and dyn_cast<> for down-casts to do something
  // useful with a Knob.

  using KnobTicket = uint64_t;

  // used to ensure knob IDs are unique.
  // we rely on the fact that 0 is an invalid knob ID
  extern std::atomic<KnobTicket> KnobTicker;

  // Base class for tunable compiler "knobs", which
  // are simply generic, tunable components.
  class Knob {
  public:
    enum KnobKind {
      KK_Int,
      KK_Flag,
      KK_Loop
    };

    Knob(KnobKind Kind) : kind_(Kind) {
      ticker_ = KnobTicker.fetch_add(1);
      assert(ticker_ != 0 && "exhausted the knob ticker!");
    }
    virtual ~Knob() = default;
    // a human-readable but unique name that idenifies this tunable knob
    virtual std::string const& getID() const = 0;

    // a unique number relative to all knobs in the current process only.
    // suitable for use in the ID of a dynamically generated Knob
    KnobTicket getTicket() const { return ticker_; }

    KnobKind getKind() const { return kind_; }

    // members related to exporting to a flat array

    virtual size_t size() const { return 1; } // num values to be flattened

  private:
    const KnobKind kind_;
    KnobTicket ticker_;

  }; // end class Knob


  // common implementation of a knob that can take on values in the range
  // [a, b], where a, b are scalar values.
  //
  // NOTE: this type is NOT represented in the LLVM-style RTTI type info.
  // it simply forwards the kind given by a subclass so that they share impl.
  template < typename ValTy >
  class ScalarKnob : public Knob {
  protected:
    ValTy Current;
    ValTy Default;
    ValTy Min;
    ValTy Max;
  public:
    ScalarKnob(KnobKind kind, ValTy current, ValTy dflt, ValTy min, ValTy max)
        : Knob(kind), Current(current), Default(dflt), Min(min), Max(max) {}

    virtual ~ScalarKnob() = default;

    // value accessors
    virtual ValTy getVal() const { return Current; }
    virtual void setVal(ValTy NewV) { Current = NewV; }

    virtual ValTy getDefault() const { return Default; }
    virtual void setDefault(ValTy NewD) { Default = NewD; }

    // inclusive ranges
    virtual ValTy getMin() const { return Min; }
    virtual void setMin(ValTy NewMin) { Min = NewMin; }

    virtual ValTy getMax() const { return Max; }
    virtual void setMax(ValTy NewMax) { Max = NewMax; }

  }; // end class ScalarKnob


  // a boolean-like scalar range
  class FlagKnob : public ScalarKnob<int> {
  private:
    static constexpr int TRUE = 1;
    static constexpr int FALSE = 0;
  public:
    virtual ~FlagKnob() = default;
    FlagKnob(bool dflt)
      : ScalarKnob<int>(KK_Flag,
                        dflt ? TRUE : FALSE, // current
                        dflt ? TRUE : FALSE, // default
                        FALSE /*min*/,  TRUE /*max*/) {}

    bool getFlag() const {
      return getVal() != FALSE;
    }

    void setFlag(bool flag) {
      setVal(flag ? TRUE : FALSE);
    }

    static bool classof(const Knob *K) {
      return K->getKind() == KK_Flag;
    }

  }; // end class FlagKnob



  class IntKnob : ScalarKnob<int> {
  public:
    IntKnob(int current, int dflt, int min, int max) :
      ScalarKnob<int>(KK_Int, current, dflt, min, max) {}

    static bool classof(const Knob *K) {
      return K->getKind() == KK_Int;
    }
  }; // end class IntKnob


} // namespace tuner
