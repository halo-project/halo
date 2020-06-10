#pragma once

#include "halo/tuner/Knob.h"

#include <unordered_map>

// These are the names of knobs that will be looked-up by the
// compilation pipeline, so they should match up with the ones specified
// in the parameter space configuration file.

namespace halo {

  /// For sanity, we tag the names with their expected runtime type tag
  ///
  /// NOTE: there is currently no functionality to ensure named knobs are used,
  /// beyond manually tracking lookups. So make sure to check for references to these
  /// globals with your IDE or something so nothing is left off!
  namespace named_knob {
    using ty = std::pair<std::string, Knob::KnobKind>;

    /// retrieves the unique name for the named knob for a specific loop id
    std::string forLoop(unsigned i, ty NamedKnob);
    std::string forLoop(unsigned i, std::string const& NamedKnob);

    static const ty IPRA            = {"ipra",             Knob::KK_Flag};
    static const ty FastISel        = {"fast-isel",        Knob::KK_Flag};
    static const ty GlobalISel      = {"global-isel",      Knob::KK_Flag};
    static const ty MachineOutline  = {"machine-outline",  Knob::KK_Flag};
    static const ty GuaranteeTCO    = {"guarantee-tco",    Knob::KK_Flag};
    static const ty InlineFullCost  = {"inline-computefullcost", Knob::KK_Flag};

    static const ty InlineThreshold = {"inline-threshold-default", Knob::KK_Int};
    static const ty InlineThresholdHint = {"inline-threshold-hintedfunc", Knob::KK_Int};
    static const ty InlineThresholdCold = {"inline-threshold-coldfunc", Knob::KK_Int};
    static const ty InlineThresholdHotSite = {"inline-threshold-hotsite", Knob::KK_Int};
    static const ty InlineThresholdColdSite = {"inline-threshold-coldsite", Knob::KK_Int};
    static const ty InlineThresholdLocalHotSite = {"inline-threshold-locallyhotsite", Knob::KK_Int};

    static const ty OptimizeLevel = {"optimize-pipeline-level", Knob::KK_OptLvl};


    ///////
    // The following options are for loops

    /// This metadata suggests an unroll factor to the loop unroller.
    static const ty LoopUnrollCount = {"llvm.loop.unroll.count", Knob::KK_Int};

    /// This metadata suggests an interleave count to the loop interleaver.
    /// Note that setting llvm.loop.interleave.count to 1 disables interleaving multiple iterations of the loop.
    /// If llvm.loop.interleave.count is set to 0 then the interleave count will be determined automatically.
    static const ty LoopInterleaveCount = {"llvm.loop.interleave.count", Knob::KK_Int};

    /// This metadata sets the target width of the vectorizer.
    /// Note that setting llvm.loop.vectorize.width to 1 disables vectorization of the loop.
    /// If llvm.loop.vectorize.width is set to 0 or if the loop does not have this metadata,
    /// the width will be determined automatically.
    static const ty LoopVectorizeWidth = {"llvm.loop.vectorize.width", Knob::KK_Int};

    /// This metadata suggests that the loop should be fully unrolled if the trip count
    /// is known at compile time and partially unrolled if the trip count is not known at compile time.
    static const ty LoopUnrollEnable = {"llvm.loop.unroll.enable", Knob::KK_Flag};

    /// This metadata disables runtime loop unrolling, which means unrolling for loops where
    /// the loop bound is a runtime value. Disabling this would basically prevent unrolling if
    /// a residual loop would be required.
    static const ty LoopRuntimeUnrollDisable = {"llvm.loop.unroll.runtime.disable", Knob::KK_Flag};


    static const std::unordered_map<std::string, Knob::KnobKind> Corpus = {
      IPRA,
      FastISel,
      GlobalISel,
      MachineOutline,
      GuaranteeTCO,
      InlineFullCost,

      InlineThreshold,
      InlineThresholdHint,
      InlineThresholdCold,
      InlineThresholdHotSite,
      InlineThresholdColdSite,
      InlineThresholdLocalHotSite,

      OptimizeLevel,

      LoopUnrollCount,
      LoopInterleaveCount,
      LoopVectorizeWidth,
      LoopUnrollEnable,
      LoopRuntimeUnrollDisable
    };

    // NB: you must add all loop options here too, or else the LoopAnnotatorPass will
    // not apply the knob's setting to the program!!
    static const std::vector<ty> LoopOptions = {
      LoopUnrollCount,
      LoopInterleaveCount,
      LoopVectorizeWidth,
      LoopUnrollEnable,
      LoopRuntimeUnrollDisable
    };

  } // end namespace named_knob
} // end namespace halo
