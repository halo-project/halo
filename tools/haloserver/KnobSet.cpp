
#include "halo/tuner/KnobSet.h"
#include "llvm/ADT/Optional.h"
#include "halo/nlohmann/util.hpp"

#include <boost/functional/hash.hpp>

#include "Logging.h"

#include <type_traits>

namespace halo {

void KnobSet::copyingUnion(const KnobSet& Other) {
  NumLoopIDs = std::max(NumLoopIDs, Other.NumLoopIDs);

  for (auto const& Entry : Other) {

    if (Knobs.find(Entry.first) != Knobs.end())
      continue; // skip! we already have a knob with this name.

    Knob* Ptr = Entry.second.get();

    if (IntKnob* K = llvm::dyn_cast<IntKnob>(Ptr))
      insert(std::make_unique<IntKnob>(*K));

    else if (FlagKnob* K = llvm::dyn_cast<FlagKnob>(Ptr))
      insert(std::make_unique<FlagKnob>(*K));

    else if (OptLvlKnob* K = llvm::dyn_cast<OptLvlKnob>(Ptr))
      insert(std::make_unique<OptLvlKnob>(*K));

    else
      fatal_error("KnobSet::KnobSet copy ctor -- unknown knob kind encountered");
  }
}

size_t KnobSet::size() const {
   return Knobs.size();
 }

void KnobSet::unsetAll() {
  for (auto &Entry : Knobs) {
    Knob *Ptr = Entry.second.get();

    if (IntKnob* K = llvm::dyn_cast<IntKnob>(Ptr))
      K->setVal(llvm::None);

    else if (FlagKnob* K = llvm::dyn_cast<FlagKnob>(Ptr))
      K->setVal(llvm::None);

    else if (OptLvlKnob* K = llvm::dyn_cast<OptLvlKnob>(Ptr))
      K->setVal(llvm::None);

    else
      fatal_error("KnobSet::unsetAll -- unknown knob kind encountered");
  }
}

size_t KnobSet::cardinality() const {
  if (Knobs.size() == 0)
    return 0;

  size_t Sz = 1;
  for (auto const& Entry : Knobs)
    Sz *= Entry.second->cardinality();

  return Sz;
}


////////////////////////////
// Knob spec parsing (from JSON) is below

std::string checkedName(JSON const& Obj, Knob::KnobKind Kind, llvm::Optional<unsigned> LoopID) {
  auto const& Corpus = named_knob::Corpus;
  auto Name = config::getValue<std::string>("name", Obj);

  auto Result = Corpus.find(Name);
  if (Result == Corpus.end())
    config::parseError("unknown knob name: " + Name);

  if (Kind != Result->second)
    config::parseError("knob with name '" + Name + "' has unexpected kind");

  if (LoopID)
    return named_knob::forLoop(LoopID.getValue(), Name);
  else
    return Name;
}


void addKnob(JSON const& Spec, KnobSet& Knobs, llvm::Optional<unsigned> LoopID) {
  //
  // Knob Specs must start with { "kind" : "KIND_NAME_HERE" ...
  //
  if (!Spec.is_object())
    config::parseError("top-level 'knobs' array must contain only "
               "objects consisting of knob specs");

  auto Kind = config::getValue<std::string>("kind", Spec);

  if (Kind == "flag") {
    auto Name = checkedName(Spec, Knob::KK_Flag, LoopID);
    std::string default_field = "default";

    // check for default: null
    // this means the flag really has 3 possible values: true, false, neither
    if (config::contains(default_field, Spec) && Spec[default_field].is_null()) {
      Knobs.insert(std::make_unique<FlagKnob>(Name));
    } else {
      // otherwise we expect a bool here
      auto Default = config::getValue<bool>(default_field, Spec, Name);
      Knobs.insert(std::make_unique<FlagKnob>(Name, Default));
    }

  } else if (Kind == "int") {
    auto Name = checkedName(Spec, Knob::KK_Int, LoopID);
    auto Min = config::getValue<int>("min", Spec, Name);
    auto Max = config::getValue<int>("max", Spec, Name);
    std::string default_field = "default";
    llvm::Optional<int> Default;

    if (!config::contains(default_field, Spec))
      config::parseError("int knob " + Name
                          + " is missing a '" + default_field
                          + "' key (you can map it to val 'null')");

    if (!Spec[default_field].is_null())
      Default = config::getValue<int>(default_field, Spec, Name);

    // interpret the required scale field
    auto ScaleName = config::getValue<std::string>("scale", Spec, Name);
    IntKnob::Scale Scale;
    if (ScaleName == "1/2") {
      Scale = IntKnob::Scale::Half;
    } else if (ScaleName == "1/100") {
      Scale = IntKnob::Scale::Hundredth;
    } else if (ScaleName == "log") {
      Scale = IntKnob::Scale::Log;
    } else if (ScaleName == "none") {
      Scale = IntKnob::Scale::None;
    } else {
      config::parseError("int knob " + Name + " has invalid 'scale' argument " + ScaleName);
    }

    Knobs.insert(std::make_unique<IntKnob>(Name, Default, Min, Max, Scale));

  } else if (Kind == "optlvl") {
    auto Name = checkedName(Spec, Knob::KK_OptLvl, LoopID);
    auto Default = config::getValue<std::string>("default", Spec, Name);
    auto Min = config::getValue<std::string>("min", Spec, Name);
    auto Max = config::getValue<std::string>("max", Spec, Name);

    Knobs.insert(std::make_unique<OptLvlKnob>(Name, Default, Min, Max));

  } else {
    config::parseError("unkown knob kind: " + Kind);
  }
}

 void KnobSet::InitializeKnobs(JSON const& Config, KnobSet& Knobs, unsigned NumLoopIDs) {
  if (!Config.is_object())
    config::parseError("top level value must be an object");

  if (!config::contains("knobs", Config))
    config::parseError("top-level object must contain a 'knobs' array");

  auto KnobSpecs = Config["knobs"];
  if (!KnobSpecs.is_array())
    config::parseError("top-level 'knobs' must be an array");

  for (auto const& Spec : KnobSpecs)
    addKnob(Spec, Knobs, llvm::None);

  // Next, if there are any loop options in the config file, we'll generate
  // knobs for them.

  KnobSpecs = Config["loopKnobs"];
  if (!KnobSpecs.is_array())
    config::parseError("top-level 'loopKnobs' must be an array");

  bool AtLeastOneLoopOption = false;
  for (auto const& Spec : KnobSpecs) {
    for (unsigned i = 0; i < NumLoopIDs; i++) {
      AtLeastOneLoopOption = true;
      addKnob(Spec, Knobs, i);
    }
  }

  if (AtLeastOneLoopOption)
    Knobs.setNumLoops(NumLoopIDs);

 }

 void KnobSet::dump() const {
  logs() << "KnobSet: {\n";

  for (auto const& Entry : *this)
    logs() << "\t" << Entry.first << " = " << Entry.second->dump() << "\n";

  logs() << "}\n";
 }


} // end namespace halo

namespace std {

  // this implementation only cares about comparing the knobs w.r.t. their current
  // values in these sets. not other aspects of knobs. This is why I have not defined
  // equality recursively in terms of Knob equality.
  size_t hash<halo::KnobSet>::operator()(halo::KnobSet const& KS) const {
    size_t seed = KS.NumLoopIDs;
    for (auto const& Entry : KS.Knobs)
      boost::hash_combine(seed, Entry.second->hashCode());
    return seed;
  }


  // this implementation only cares about comparing the knobs w.r.t. their current
  // values in these sets. not other aspects of knobs. This is why I have not defined
  // equality recursively in terms of Knob equality.
  bool equal_to<halo::KnobSet>::operator()(halo::KnobSet const& A, halo::KnobSet const& B) const {
    if (A.NumLoopIDs != B.NumLoopIDs)
      return false;

    if (A.size() != B.size())
      return false;

    for (auto const& EntryA : A.Knobs) {
      auto ResultB = B.Knobs.find(EntryA.first);

      if (ResultB == B.Knobs.end())
        return false;

      halo::Knob const* PtrA = EntryA.second.get();
      halo::Knob const* PtrB = ResultB->second.get();

      if (PtrA->getKind() != PtrB->getKind())
        return false;

      if (halo::IntKnob const* IK_A = llvm::dyn_cast<halo::IntKnob>(PtrA))
        if (halo::IntKnob const* IK_B = llvm::dyn_cast<halo::IntKnob>(PtrB)) {
          if (IK_A->sameValAs(IK_B))
            continue;
          else
            return false;
        }

      if (halo::FlagKnob const* FK_A = llvm::dyn_cast<halo::FlagKnob>(PtrA))
        if (halo::FlagKnob const* FK_B = llvm::dyn_cast<halo::FlagKnob>(PtrB)) {
          if (FK_A->sameValAs(FK_B))
            continue;
          else
            return false;
        }


      if (halo::OptLvlKnob const* OK_A = llvm::dyn_cast<halo::OptLvlKnob>(PtrA))
        if (halo::OptLvlKnob const* OK_B = llvm::dyn_cast<halo::OptLvlKnob>(PtrB)) {
          if (OK_A->sameValAs(OK_B))
            continue;
          else
            return false;
        }

      halo::fatal_error("Knob kinds are identical but dyn_casts failed? Perhaps you forgot to update std::equal_to");
    }

    return true;
  }
} // namespace std
