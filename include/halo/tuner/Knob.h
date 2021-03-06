#pragma once

#include <cinttypes>
#include <limits>
#include <string>
#include <cassert>
#include <atomic>
#include <cmath>

#include "halo/tuner/Utility.h"

#include "llvm/Support/Casting.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/ADT/Optional.h"

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

    // the number of options this knob can be set to.
    virtual size_t cardinality() const = 0;

    // a unique number relative to all knobs in the current process only.
    // suitable for use in the ID of a dynamically generated Knob
    KnobTicket getTicket() const { return ticker_; }

    KnobKind getKind() const { return kind_; }

    // returns a hashed value of the current knob's setting
    // that is suitable for use in STL unordered containers
    virtual size_t hashCode() const = 0;

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
    llvm::Optional<ValTy> Current;
    ValTy Min;
    ValTy Max;
  public:
    ScalarKnob(KnobKind kind, std::string const& name, llvm::Optional<ValTy> current, ValTy min, ValTy max)
        : Knob(kind), Name(name),
          Current(current), Min(min), Max(max) {
            if (Current.hasValue())
              if (!(Min <= Current.getValue() && Current.getValue() <= Max))
                fatal_error("ScalarKnob Ctor -- contract that min <= default <= max violated.");
          }

    virtual ~ScalarKnob() = default;

    std::string const& getID() const override {
      return Name;
    }

    // main value accessor. assignment only occurs if the knob is set.
    template <typename ValTyAssignable>
    void applyVal(ValTyAssignable& Out) const {
      if (hasVal())
        Out = Current.getValue();
    }

    // alternate accessor. provided func is applied to the value if the knob is set.
    void applyVal(std::function<void(ValTy)> AssignAction) const {
      if (hasVal())
        AssignAction(Current.getValue());
    }

    // assign or clear the current setting of this knob.
    void setVal(llvm::Optional<ValTy> NewV) {
      assert(!NewV.hasValue() || (getMin() <= NewV.getValue() && NewV.getValue() <= getMax()));
      Current = NewV;
    }

    bool hasVal() const { return Current.hasValue(); }

    // compares the given knob's current setting for equality
    // with this knob's current setting.
    bool sameValAs(ScalarKnob<ValTy> const* Other) const {
      return Current == Other->Current;
    }

    // inclusive ranges
    ValTy getMin() const { return Min; }
    ValTy getMax() const { return Max; }

    // compares the raw current value of this knob with the provided value.
    // if the knob is unset / has no value, then false is returned.
    bool equalsUnscaled(ValTy Val) const {
      if (!hasVal())
        return false;
      return Current.getValue() == Val;
    }

  }; // end class ScalarKnob


  // a boolean-like scalar range, which can also be neither true or false
  // depending on the constructor.
  class FlagKnob : public ScalarKnob<int> {
  private:
    bool HadDefault; // cruft for loop knobs, sadly.
  public:
    static constexpr int TRUE = 1;
    static constexpr int FALSE = 0;

    virtual ~FlagKnob() = default;

    FlagKnob(std::string const& Name)
      : ScalarKnob<int>(KK_Flag, Name,
                        llvm::None, // current
                        FALSE /*min*/,  TRUE /*max*/), HadDefault(false) {}

    FlagKnob(std::string const& Name, bool dflt)
      : ScalarKnob<int>(KK_Flag, Name,
                        dflt ? TRUE : FALSE, // current
                        FALSE /*min*/,  TRUE /*max*/), HadDefault(true) {}

    // Performs an assignment to the reference passed in, only if the
    // flag is either true or false. No assignment occurs if the flag is 'neither'.
    template <typename BoolAssignable>
    void applyFlag(BoolAssignable &Option) const {
      applyFlag([&](bool Val) {
        Option = Val;
      });
    }

    // fun fact: this version exists b/c you can't pass a reference to a bit field.
    void applyFlag(std::function<void(bool)> &&AssignAction) const {
      applyVal([&](int Val) {
        AssignAction(Val == TRUE);
      });
    }

    bool hadDefault() const { return HadDefault; }
    bool isTrue() const { return Current.getValueOr(FALSE) == TRUE; }
    bool isNeither() const { return !hasVal(); }

    void setFlag(bool Val) {
      Current = (Val ? TRUE : FALSE);
    }

    size_t cardinality() const override {
      // true, false, neither
      return 2 + 1;
    }

    size_t hashCode() const override {
      return Current.getValueOr(getMax()+1);
    }

    static bool classof(const Knob *K) {
      return K->getKind() == KK_Flag;
    }

    std::string dump() const override {
      std::string AsStr = "none";
      applyFlag([&](bool Flag) {
        if (Flag)
          AsStr = "true";
        else
          AsStr = "false";
      });
      return AsStr;
    }

  }; // end class FlagKnob

  class OptLvlKnob : public ScalarKnob<llvm::PassBuilder::OptimizationLevel> {
  public:
    using LevelTy = llvm::PassBuilder::OptimizationLevel;
    OptLvlKnob(std::string const& Name, std::string const& current,
               std::string const& min, std::string const& max)
      : ScalarKnob<LevelTy>(KK_OptLvl, Name,
          parseLevel(current), parseLevel(min), parseLevel(max)) {}

    std::string dump() const override {
      LevelTy Val;
      applyVal(Val);
      return std::to_string(OptLvlKnob::asInt(Val));
    }

    void applyCodegenLevel(llvm::CodeGenOpt::Level &Out) const {
      applyVal([&](LevelTy Lvl) {
        switch(asInt(Lvl)) {
          case 0: Out = llvm::CodeGenOpt::None; break;
          case 1: Out = llvm::CodeGenOpt::Less; break;
          case 2: Out = llvm::CodeGenOpt::Default; break;
          case 3: Out = llvm::CodeGenOpt::Aggressive; break;
          default: fatal_error("impossible level");
        };
      });
    }

    size_t cardinality() const override {
      // normal number of options + 1 for the "none" setting
      return (asInt(getMax()) - asInt(getMin()) + 1) + 1;
    }

    size_t hashCode() const override {
      if (Current.hasValue())
        return asInt(Current.getValue());
      return -1;
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
  public:
    enum class Scale {
      None,       // 1:1
      Log,        // the knob's values are log_2 of the actual values
      Half,       // the knob's values are 1/2 the actual values
      Hundredth   // the knob's values are 1/100 the actual values
    };

    IntKnob(std::string const& Name, llvm::Optional<int> current, int min, int max, Scale scale) :
      ScalarKnob<int>(KK_Int, Name, current, min, max), ScaleKind(scale) {}

    // Accesses the "actual" value that this knob represents,
    // accounting for any scaling.
    void applyScaledVal(std::function<void(int)> &&AssignTo) const {
      applyVal([&](int Val) {
        if (ScaleKind == Scale::Log) {

          if (Val < 0)
            AssignTo(0);   // [-inf, -1] --> 0
          else
            AssignTo(std::pow(2, Val));  // [0, inf] --> 2^(val)

        } else if (ScaleKind == Scale::Half) {
          AssignTo(2 * Val);

        } else if (ScaleKind == Scale::Hundredth) {
          AssignTo(100 * Val);

        } else {
          assert(ScaleKind == Scale::None);
          AssignTo(Val);
        }
      });
    }

    size_t cardinality() const override {
      // normal number of options, +1 for the none option
      return (getMax() - getMin() + 1) + 1;
    }

    size_t hashCode() const override {
      return Current.getValueOr(getMax()+1);
    }

    template <typename IntAssignable>
    void applyScaledVal(IntAssignable &Out) const {
      applyScaledVal([&](int Ans){ Out = Ans; });
    }

    std::string dump() const override {
      if (!hasVal())
        return "none";

      int ScaledVal;
      applyScaledVal(ScaledVal);
      return std::to_string(ScaledVal);
    }

    static bool classof(const Knob *K) {
      return K->getKind() == KK_Int;
    }

  private:
    Scale ScaleKind;
  }; // end class IntKnob

} // end namespace halo
