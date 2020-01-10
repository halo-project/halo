#pragma once

#include "halo/Knob.h"
#include "halo/Utility.h"

#include "llvm/ADT/Optional.h"
#include "llvm/Support/Error.h"

#include <cinttypes>
#include <random>

namespace halo {

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"

  namespace loop_md {
    // make sure to keep this in sync with MDUtils.h
    static char const* TAG = "llvm.loop.id";

    static char const* UNROLL_DISABLE = "llvm.loop.unroll.disable";
    static char const* UNROLL_COUNT = "llvm.loop.unroll.count";
    static char const* UNROLL_FULL = "llvm.loop.unroll.full";
    static char const* VECTORIZE_ENABLE = "llvm.loop.vectorize.enable";
    static char const* VECTORIZE_WIDTH = "llvm.loop.vectorize.width";
    static char const* LICM_VER_DISABLE = "llvm.loop.licm_versioning.disable";
    static char const* INTERLEAVE_COUNT = "llvm.loop.interleave.count";
    static char const* DISTRIBUTE = "llvm.loop.distribute.enable";
    static char const* SECTION = "llvm.loop.tile";
  }

#pragma GCC diagnostic pop

  // https://llvm.org/docs/LangRef.html#llvm-loop
  //
  // NOTE if you add a new option here, make sure to update:
  // 1. LoopKnob.cpp::addToLoopMD
  //    1a. You might need to update MDUtils.h while doing this.
  // 2. operator<<(stream, LoopSetting) and operator== in LoopKnob.cpp
  // 3. any generators of a LoopSetting,
  //    like genRandomLoopSetting or genNearbyLoopSetting
  //
  struct LoopSetting {
    // hints only
    llvm::Optional<uint16_t> VectorizeWidth{}; // 1 = disable entirely, >= 2 suggests width

    // hint only
    llvm::Optional<uint16_t> InterleaveCount{}; // 0 = off, 1 = automatic, >=2 is count.

    // TODO: these 3 ought to be combined into 1 integer option
    llvm::Optional<bool> UnrollDisable{};    // llvm.loop.unroll.disable
    llvm::Optional<bool> UnrollFull{};       // llvm.loop.unroll.full
    llvm::Optional<uint16_t> UnrollCount{};  // llvm.loop.unroll.count

    llvm::Optional<bool> LICMVerDisable{}; // llvm.loop.licm_versioning.disable

    llvm::Optional<bool> Distribute{};

    size_t size() const {
      return 7;
    }

    static void flatten(float* slice, LoopSetting LS) {
      size_t i = 0;

      LoopSetting::flatten(slice + i++, LS.VectorizeWidth);

      LoopSetting::flatten(slice + i++, LS.InterleaveCount);

      LoopSetting::flatten(slice + i++, LS.UnrollDisable);
      LoopSetting::flatten(slice + i++, LS.UnrollFull);
      LoopSetting::flatten(slice + i++, LS.UnrollCount);

      LoopSetting::flatten(slice + i++, LS.LICMVerDisable);

      LoopSetting::flatten(slice + i++, LS.Distribute);

      if (i != LS.size())
        llvm::report_fatal_error("size does not match expectations");
    }

    static void flatten(float* slice, llvm::Optional<bool> opt) {
      if (opt)
        *slice = opt.hasValue() ? 1.0 : 0.0;
      else
        *slice = MISSING;
    }

    static void flatten(float* slice, llvm::Optional<uint16_t> opt) {
      if (opt)
        *slice = (float) opt.hasValue();
      else
        *slice = MISSING;
    }

  };

  class LoopKnob : public Knob<LoopSetting> {
  private:
    LoopSetting Opt;
    unsigned LoopID;
    unsigned nestingDepth;
    std::vector<LoopKnob*> kids;
    std::string Name;

    // NOTE could probably add some utilities to check the
    // sanity of a loop setting to this class?


  public:
    LoopKnob (unsigned name, std::vector<LoopKnob*> children_, unsigned depth_)
      : LoopID(name),
        nestingDepth(depth_),
        kids(std::move(children_)),
        Name("loop #" + std::to_string(LoopID)) {}

    LoopSetting getDefault() const override {
      LoopSetting Empty;
      return Empty;
    }

    // loop structure information
    std::vector<LoopKnob*>& children() { return kids; }
    auto begin() { return kids.begin(); }
    auto end() { return kids.end(); }
    unsigned loopDepth() const { return nestingDepth; }

    LoopSetting getVal() const override { return Opt; }

    void setVal (LoopSetting LS) override { Opt = LS; }

    unsigned getLoopName() const { return LoopID; }

    virtual std::string const& getID() const override {
       return Name;
    }

    virtual size_t size() const override {
      return Opt.size();
    }

  }; // end class


// any specializations of genRandomLoopSetting you would like to use
// should be declared as an extern template here, and then instantiated
// in LoopSettingGen, since I don't want to include the generic impl here.
// see: https://stackoverflow.com/questions/10632251/undefined-reference-to-template-function
template < typename RNE >  // meets the requirements of RandomNumberEngine
LoopSetting genRandomLoopSetting(RNE &Eng);

extern template
LoopSetting genRandomLoopSetting<std::mt19937_64>(std::mt19937_64&);


template < typename RNE >
LoopSetting genNearbyLoopSetting(RNE &Eng, LoopSetting LS, double energy);

extern template
LoopSetting genNearbyLoopSetting<std::mt19937_64>(std::mt19937_64&, LoopSetting, double);

} // namespace tuner

std::ostream& operator<<(std::ostream &o, halo::LoopSetting &LS);
bool operator==(halo::LoopSetting const& A, halo::LoopSetting const& B);
bool operator!=(halo::LoopSetting const& A, halo::LoopSetting const& B);
