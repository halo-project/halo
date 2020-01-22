
#include "halo/tuner/KnobSet.h"
#include "halo/nlohmann/json.hpp"

#include "Logging.h"

#include <type_traits>

namespace halo {

KnobSet::KnobSet(const KnobSet& Other) {
  for (auto const& Entry : Other) {
    Knob* Ptr = Entry.second.get();

    if (IntKnob* K = llvm::dyn_cast<IntKnob>(Ptr))
      insert(std::make_unique<IntKnob>(*K));

    else if (FlagKnob* K = llvm::dyn_cast<FlagKnob>(Ptr))
      insert(std::make_unique<FlagKnob>(*K));

    else if (LoopKnob* K = llvm::dyn_cast<LoopKnob>(Ptr))
      insert(std::make_unique<LoopKnob>(*K));

    else if (OptLvlKnob* K = llvm::dyn_cast<OptLvlKnob>(Ptr))
      insert(std::make_unique<OptLvlKnob>(*K));

    else
      llvm::report_fatal_error("KnobSet::KnobSet copy ctor -- unknown knob kind encountered");
  }
}

size_t KnobSet::size() const {
   size_t numVals = 0;

   for (auto const& Entry : Knobs)
     numVals += Entry.second->size();

   return numVals;
 }


////////////////////////////
// Knob spec parsing (from JSON) is below

void parseError(std::string const& hint) {
  llvm::report_fatal_error("Error during parsing of config file:\n\t" + hint);
}

inline bool contains(std::string const& Key, JSON const& Object) {
  return Object.find(Key) != Object.end();
}

// checked lookup of a value for the given key.
template <typename T>
T getValue(std::string const& Key, JSON const& Root, std::string Context = "") {
  if (Context != "")
    Context = "(" + Context + ") ";

  if (!contains(Key, Root))
    parseError(Context + "expected member '" + Key + "' not found.");

  auto Obj = Root[Key];

  if (std::is_same<T, bool>()) {
    if (!Obj.is_boolean())
      parseError(Context + "member '" + Key + "' must be a boolean.");

  } else if (std::is_integral<T>() || std::is_floating_point<T>()) {
    if (!Obj.is_number())
      parseError(Context + "member '" + Key + "' must be a number.");

  } else if (std::is_same<T, std::string>()) {
    if (!Obj.is_string())
      parseError(Context + "member '" + Key + "' must be a string.");

  } else {
    llvm::report_fatal_error("internal error -- unhandled getValue type case");
  }

  return Obj.get<T>();
}

std::string checkedName(JSON const& Obj, Knob::KnobKind Kind) {
  auto const& Corpus = named_knob::Corpus;
  auto Name = getValue<std::string>("name", Obj);

  auto Result = Corpus.find(Name);
  if (Result == Corpus.end())
    parseError("unknown knob name: " + Name);

  if (Kind != Result->second)
    parseError("knob with name '" + Name + "' has unexpected kind");

  return Name;
}


void addKnob(JSON const& Spec, KnobSet& Knobs) {
  //
  // Knob Specs must start with { "kind" : "KIND_NAME_HERE" ...
  //
  if (!Spec.is_object())
    parseError("top-level 'knobs' array must contain only "
               "objects consisting of knob specs");

  auto Kind = getValue<std::string>("kind", Spec);

  if (Kind == "flag") {
    auto Name = checkedName(Spec, Knob::KK_Flag);
    auto Default = getValue<bool>("default", Spec, Name);

    Knobs.insert(std::make_unique<FlagKnob>(Name, Default));


  } else if (Kind == "int") {
    auto Name = checkedName(Spec, Knob::KK_Int);
    auto Default = getValue<int>("default", Spec, Name);
    auto Min = getValue<int>("min", Spec, Name);
    auto Max = getValue<int>("max", Spec, Name);
    auto Scale = getValue<bool>("log_scale", Spec, Name);

    Knobs.insert(std::make_unique<IntKnob>(Name, Default, Default, Min, Max, Scale));

  } else if (Kind == "optlvl") {
    auto Name = checkedName(Spec, Knob::KK_OptLvl);
    auto Default = getValue<std::string>("default", Spec, Name);
    auto Min = getValue<std::string>("min", Spec, Name);
    auto Max = getValue<std::string>("max", Spec, Name);

    Knobs.insert(std::make_unique<OptLvlKnob>(Name, Default, Default, Min, Max));

  } else {
    parseError("unkown knob kind: " + Kind);
  }
}

 void KnobSet::InitializeKnobs(JSON const& Config, KnobSet& Knobs) {
  if (!Config.is_object())
    parseError("top level value must be an object");

  if (!contains("knobs", Config))
    parseError("top-level object must contain a 'knobs' array");

  auto KnobSpecs = Config["knobs"];
  if (!KnobSpecs.is_array())
    parseError("top-level 'knobs' must be an array");

  for (auto const& Spec : KnobSpecs)
    addKnob(Spec, Knobs);

 }

} // end namespace halo