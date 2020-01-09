#pragma once

#include <cinttypes>
#include <climits>
#include <string>
#include <cassert>
#include <atomic>

#include <halo/Utility.h>

#include "llvm/IR/Module.h"
#include "llvm/Support/Casting.h"

namespace halo {

  // polymorphism of Knobs is primarily achieved through inheritance
  // via LLVM-style RTTI:
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
  public:
    enum KnobKind {
      KK_ScalarKnob
    };

    Knob(KnobKind K) : Kind(K) {
      ID = KnobTicker.fetch_add(1);
      assert(ID != 0 && "exhausted the knob ticker!");
    }
    virtual ~Knob() = default;

    // a unique ID relative to all knobs in the process.
    // since multiple instances of autotuners can be created
    // per process, this only guarentees uniqueness of each
    // instance, it is otherwise unstable.
    KnobID getID() const { return ID; }

    KnobKind getKind() const { return Kind; }

    // virtual value accessors
    virtual std::string const& getName() const = 0;
    virtual ValTy getDefault() const = 0;
    virtual ValTy getVal() const = 0;
    virtual void setVal(ValTy) = 0;

    // num values to be flattened
    // virtual size_t size() const { return 1; }

  private:
    KnobID ID;
    const KnobKind Kind;

  }; // end class Knob


} // end namespace halo
