#pragma once

#include "halo/tuner/NamedKnobs.h"
#include "halo/nlohmann/json_fwd.hpp"

#include "Logging.h"

#include <unordered_map>
#include <functional>

using JSON = nlohmann::json;

namespace halo {
  class KnobSet;
} // namespace halo


// these comparison and hash functions only care about the
// current setting of each knob in the set to help deduplicate
// identical configurations of knobs. They're not
// useful for deep equality of configuration spaces (to save time).
namespace std {
  template<>
  struct hash<halo::KnobSet> {
    size_t operator()(halo::KnobSet const&) const;
  };

  template<>
  struct equal_to<halo::KnobSet> {
    bool operator()(halo::KnobSet const&, halo::KnobSet const&) const;
  };
} // namespace std


namespace halo {

  // handy type aliases.
  namespace knob_ty {
    using Int = ScalarKnob<int>;
  }


  class KnobSet {
  private:
    std::unordered_map<std::string, std::unique_ptr<Knob>> Knobs;
    unsigned NumLoopIDs{0};

  public:

    KnobSet() = default;
    KnobSet(KnobSet&&) = default;
    KnobSet& operator=(KnobSet&&) = default;

    KnobSet(const KnobSet& Other) {
      assert(Knobs.empty() && "not starting from an empty set?");
      copyingUnion(Other);
    }

    // A fairly standard set-union operation, where
    //
    //    this = this `unionWith` other
    //
    // The difference are that:
    //
    // 1. knobs are uniqued by their name (aka their ID)
    // 2. when inserting missing knobs into this
    // knob set, we insert a copy of the knob from the other set.
    void copyingUnion(const KnobSet&);

    // an insertion operation that assumes a knob with the given name
    // does NOT already exist in the set.
    Knob& insert(std::unique_ptr<Knob> KNB) {
      auto Name = KNB->getID();
      auto Res = Knobs.insert({Name, std::move(KNB)});
      auto Iter = Res.first;
      bool InsertionOccured = Res.second;
      if (!InsertionOccured) fatal_error(
                                  "Tried to add knob with name '" + Name +
                                  "' that already exists in the set!");
      return *(Iter->second);
    }

    template <typename T>
    T& lookup(named_knob::ty const& Name) const {
      return lookup<T>(Name.first);
    }

    // Performs a lookup for the given knob name having the specified type,
    // crashing if either one fails.
    template <typename T>
    T& lookup(std::string const& Name) const {
      auto Search = Knobs.find(Name);
      if (Search != Knobs.end()) {
        Knob* Ptr = Search->second.get();

        if (T* Derived = llvm::dyn_cast<T>(Ptr))
          return *Derived;
        else
          fatal_error("unexpected type for knob " + Name + " during lookup");
      }

      fatal_error("unknown knob name requested: " + Name);
    }

    void setNumLoops(unsigned Sz) { NumLoopIDs = Sz; }
    unsigned getNumLoops() const { return NumLoopIDs; }

    // all knobs will be set to 'none',
    // for those that support it.
    void unsetAll();

    // returns the size of the configuration space induced by this set of knobs.
    size_t cardinality() const;


    auto begin() noexcept { return Knobs.begin(); }
    auto begin() const noexcept { return Knobs.cbegin(); }

    auto end() noexcept { return Knobs.end(); }
    auto end() const noexcept { return Knobs.cend(); }

    size_t size() const;

    void dump() const;

    static void InitializeKnobs(JSON const&, KnobSet&, unsigned NumLoopIDs);

    friend size_t std::hash<KnobSet>::operator()(KnobSet const&) const;
    friend bool std::equal_to<KnobSet>::operator()(KnobSet const&, KnobSet const&) const;

  };

} // namespace halo