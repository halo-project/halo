#pragma once

#include "halo/Knob.h"

// These are the names of knobs that will be looked-up by the
// compilation pipeline, so they should match up with the ones specified
// in the parameter space configuration file.

namespace halo {

  // for sanity, we tag the names with their expected runtime type tag
  namespace named_knob {
    using ty = std::pair<std::string, Knob::KnobKind>;

    static const ty IPRA            = {"ipra",             Knob::KK_Flag};
    static const ty FastISel        = {"fast-isel",        Knob::KK_Flag};
    static const ty GlobalISel      = {"global-isel",      Knob::KK_Flag};
    static const ty MachineOutline  = {"machine-outline",  Knob::KK_Flag};
    static const ty GuaranteeTCO    = {"guarantee-tco",    Knob::KK_Flag};

    static const ty InlineThreshold = {"inline-threshold", Knob::KK_Int};

    static const ty OptimizeLevel = {"optimize-pipeline-level", Knob::KK_OptLvl};

  } // end namespace named_knob
} // end namespace halo
