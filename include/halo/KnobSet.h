#pragma once

#include "halo/Knob.h"
#include "halo/LoopKnob.h"

#include <unordered_map>

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

    Knob& lookup(std::string const& Name) {
      auto Search = Knobs.find(Name);
      if (Search != Knobs.end())
        return *Search->second;
    }

    auto begin() noexcept { return Knobs.begin(); }
    auto begin() const noexcept { return Knobs.begin(); }

    auto end() noexcept { return Knobs.end(); }
    auto end() const noexcept { return Knobs.end(); }

    size_t size() const;

  };

}
