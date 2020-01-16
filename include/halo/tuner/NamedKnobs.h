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

      OptimizeLevel
    };

  } // end namespace named_knob
} // end namespace halo
