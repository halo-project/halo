#pragma once

#include <cinttypes>
#include <limits>
#include <string>
#include <cassert>
#include <atomic>
#include <cmath>

#include "halo/tuner/Utility.h"

#include "llvm/Support/Casting.h"
#include "llvm/Passes/PassBuilder.h"

#include "Logging.h"

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
      KK_OptLvl
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

    // the knob's current value as a string (for debugging).
    virtual std::string dump() const { return "?"; }

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
    std::string Name;
    ValTy Current;
    ValTy Default;
    ValTy Min;
    ValTy Max;
  public:
    ScalarKnob(KnobKind kind, std::string const& name, ValTy current, ValTy dflt, ValTy min, ValTy max)
        : Knob(kind), Name(name),
          Current(current), Default(dflt), Min(min), Max(max) {
            if (!(Min <= Default && Default <= Max))
              llvm::report_fatal_error("ScalarKnob Ctor -- contract that min <= default <= max violated.");
          }

    virtual ~ScalarKnob() = default;

    std::string const& getID() const override {
      return Name;
    }

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
    FlagKnob(std::string const& Name, bool dflt)
      : ScalarKnob<int>(KK_Flag, Name,
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

    std::string dump() const override {
      return getFlag() ? "true" : "false";
    }

  }; // end class FlagKnob

  class OptLvlKnob : public ScalarKnob<llvm::PassBuilder::OptimizationLevel> {
  public:
    using LevelTy = llvm::PassBuilder::OptimizationLevel;
    OptLvlKnob(std::string const& Name, std::string const& current,
               std::string const& dflt, std::string const& min, std::string const& max)
      : ScalarKnob<LevelTy>(KK_OptLvl, Name,
          parseLevel(current), parseLevel(dflt), parseLevel(min), parseLevel(max)) {}

    std::string dump() const override {
      return std::to_string(OptLvlKnob::asInt(getVal()));
    }

    static LevelTy parseLevel(std::string const& Level) {
      if (Level == "O0")
        return LevelTy::O0;

      if (Level == "O1")
        return LevelTy::O1;

      if (Level == "O2")
        return LevelTy::O2;

      if (Level == "O3")
        return LevelTy::O3;

      fatal_error("invalid opt level string: " + Level);
    }

    static LevelTy parseLevel(unsigned Level) {
      if (Level == 0)
        return LevelTy::O0;

      if (Level == 1)
        return LevelTy::O1;

      if (Level == 2)
        return LevelTy::O2;

      if (Level == 3)
        return LevelTy::O3;

      fatal_error("invalid opt level int: " + std::to_string(Level));
    }

    static unsigned asInt(LevelTy Level) {
      if (Level == LevelTy::O0)
        return 0;

      if (Level == LevelTy::O1)
        return 1;

      if (Level == LevelTy::O2)
        return 2;

      if (Level == LevelTy::O3)
        return 3;

      fatal_error("invalid opt level given in asInt");
    }

    static bool classof(const Knob *K) {
      return K->getKind() == KK_OptLvl;
    }

  }; // end class OptLvlKnob

bool operator <= (OptLvlKnob::LevelTy const& a, OptLvlKnob::LevelTy const& b);



  class IntKnob : public ScalarKnob<int> {
  private:
    bool LogScale;
  public:
    IntKnob(std::string const& Name, int current, int dflt, int min, int max, bool logscale) :
      ScalarKnob<int>(KK_Int, Name, current, dflt, min, max), LogScale(logscale) {}

    // indicates whether the values backing this knob are log_2 of
    // the actual values.
    bool isLogScale() const { return LogScale; }

    // Returns the "actual" value that this knob represents,
    // accounting for any scaling.
    int getScaledVal() const {
      if (isLogScale())
        return std::pow(2, getVal());

      return getVal();
    }

    std::string dump() const override {
      return std::to_string(getScaledVal());
    }

    static bool classof(const Knob *K) {
      return K->getKind() == KK_Int;
    }
  }; // end class IntKnob

} // end namespace halo
