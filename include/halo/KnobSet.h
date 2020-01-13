#pragma once

#include "halo/Knob.h"
#include "halo/LoopKnob.h"
#include "halo/NamedKnobs.h"
#include "halo/nlohmann/json_fwd.hpp"

#include <unordered_map>

using JSON = nlohmann::json;

namespace halo {

  // handy type aliases.
  namespace knob_ty {
    using Loop = LoopKnob;
    using Int = ScalarKnob<int>;
  }


  class KnobSet {
  private:
    std::unordered_map<std::string, std::unique_ptr<Knob>> Knobs;

  public:

    KnobSet() {}
    KnobSet(const KnobSet&);

    Knob& insert(std::unique_ptr<Knob> KNB) {
      auto Name = KNB->getID();
      auto Res = Knobs.insert({Name, std::move(KNB)});
      auto Iter = Res.first;
      bool InsertionOccured = Res.second;
      if (!InsertionOccured) llvm::report_fatal_error("Tried to add knob "
                              "with name that already exists in the set!");
      return *(Iter->second);
    }

    template <typename T>
    T& lookup(named_knob::ty const& Name) const {
      return lookup<T>(Name.first);
    }

    // Performs a lookup for the given knob name having the specified type,
    // crashing if either one fails.
    // FIXME: make it return an option type instead.
    template <typename T>
    T& lookup(std::string const& Name) const {
      auto Search = Knobs.find(Name);
      if (Search != Knobs.end()) {
        Knob* Ptr = Search->second.get();

        if (T* Derived = llvm::dyn_cast<T>(Ptr))
          return *Derived;
        else
          llvm::report_fatal_error("unexpected type for knob during lookup");
      }

      llvm::report_fatal_error("unknown knob name requested");
    }

    auto begin() noexcept { return Knobs.begin(); }
    auto begin() const noexcept { return Knobs.begin(); }

    auto end() noexcept { return Knobs.end(); }
    auto end() const noexcept { return Knobs.end(); }

    size_t size() const;

    static void InitializeKnobs(JSON const&, KnobSet&);

  };

}
