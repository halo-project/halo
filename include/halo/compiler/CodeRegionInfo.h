#pragma once

#include <vector>
#include <unordered_map>

#include "llvm/ADT/Optional.h"

// Function interface reference:
// https://www.boost.org/doc/libs/1_65_0/libs/icl/doc/html/boost_icl/interface/function_synopsis.html

// [NOTE identity element]
// https://www.boost.org/doc/libs/1_65_0/libs/icl/doc/html/boost_icl/concepts/map_traits.html#boost_icl.concepts.map_traits.definedness_and_storage_of_identity_elements

#define BOOST_ICL_USE_STATIC_BOUNDED_INTERVALS
#include "boost/icl/interval_map.hpp"

#include "Messages.pb.h"
#include "Logging.h"

namespace icl = boost::icl;

namespace halo {

class PerformanceData;

struct FunctionDefinition {
  std::string Name;
  bool Patchable;
  uint64_t Start;
  uint64_t End;

  FunctionDefinition(std::string name, bool patchable, uint64_t start, uint64_t end)
    : Name(name), Patchable(patchable), Start(start), End(end) {}

  bool isKnown() const { return Start != 0; }
  bool isUnknown() const { return !(isKnown()); }

  void dump(llvm::raw_ostream &out) const {
    out << "{name = " << Name
        << ", patchable = " << Patchable
        << ", start = " << Start
        << ", end = " << End << "}\n";
  }
};

class FunctionInfo {
public:
  FunctionInfo(FunctionDefinition const& Def) {
    addDefinition(Def);
  }

  /// the function's canonical label / name.
  /// This name is the one that appears first defined first in the FunctionInfo.
  /// It's generally not sufficient or useful for comparisons because it's arbitrary
  /// what name appears first!
  std::string const& getCanonicalName() const {
    assert(!FD.empty() && "did not expect an empty definition list!");
    return FD[0].Name;
  }

  /// @returns true iff the given function name matches one of the names
  /// this function is known by.
  bool knownAs(std::string const& other) const {
    for (auto const& D : FD)
      if (D.Name == other)
        return true;

    return false;
  }

  /// @returns true iff a name within this FunctionInfo appears
  /// in the given FunctionInfo.
  bool matchingName(FunctionInfo const* Other) {
    // Ugh, O(n^2) b/c matchesName is O(n)
    for (auto const& D : FD)
      if (Other->knownAs(D.Name))
        return true;

    return false;
  }

  // whether this client can patch this function
  // at all of the definitions currently in the process.
  bool isPatchable() const {
    assert(!FD.empty() && "did not expect an empty definition list!");

    for (auto const& D : FD)
      if (D.Patchable == false)
        return false;

    return true;
  }

  /// @returns true iff the given IP is equal to one of
  /// the starting addresses for a definition of this function.
  bool hasStart(uint64_t IP) const {
    for (auto const& D : FD)
      if (D.Start == IP)
        return true;

    return false;
  }

  /// @returns all of the definitions available for this function.
  /// this collection is empty if the function is unknown.
  std::vector<FunctionDefinition> const& getDefinitions() const {
    return FD;
  }

  void addDefinition(FunctionDefinition const& D) {
    FD.push_back(D);
  }

  /// @returns true iff the function has zero known definitions.
  bool isUnknown() const {
    for (auto const& D : FD)
      if (D.isKnown())
        return false;

    return true;
  }
  bool isKnown() const { return !(isUnknown()); }

  void dump(llvm::raw_ostream &out) const {
    out << "FunctionInfo = [";

    for (auto const& D : FD) {
      D.dump(out);
      out << ", ";
    }

    out << "]\n";
  }

private:
  std::vector<FunctionDefinition> FD;
};


// Provides client-specific information about the code regions within
// its process.
//
// We are currently assuming only one code region. Some old infrastructure
// had multiple disjoint code regions in mind (e.g., handle shared
// libs with bitcode). That's not currently a priority.
class CodeRegionInfo {
private:
  using CodeMap = icl::interval_map<uint64_t, FunctionInfo*,
                                    icl::partial_enricher>;

  // Either map can be used to lookup the same function information pointer.
  //
  // The NameMap includes UnknownFn for convenience, but the AddrMap does _not_.
  CodeMap AddrMap;
  std::unordered_map<std::string, FunctionInfo*> NameMap;
  uint64_t VMABase;

  // fixed name for the unknown function, which should be impossible
  // for a function name to ever be.
  static const std::string UnknownFn;

  // A special representation of unknown functions. The FunctionInfo for this
  // "function" is returned on lookup failure.
  FunctionInfo *UnknownFI;

public:
  // performs the actual initialization of the CRI based on the client enrollment
  void init(pb::ClientEnroll const& CE);

  // returns the unknown function's info
  FunctionInfo* getUnknown() const { return UnknownFI; }

  // Upon failure to lookup information for the given IP or Name, the
  // UnknownFI is returned.
  FunctionInfo* lookup(uint64_t IP) const;
  FunctionInfo* lookup(std::string const& Name) const;
  void addRegion(FunctionDefinition const&);

  /// returns true if the branch from source to target is considered
  /// a function call. There are are few situations:
  ///
  ///   1. Source & Target are in different function bodies
  ///
  ///   2. Source originates within function A, and Target is the start of Function A.
  ///
  ///   3. The source is NOT an unknown function but the target IS unknown. (& vice versa!)
  ///
  bool isCall(uint64_t Source, uint64_t Target) const;

  auto const& getNameMap() { return NameMap; }

  /// initializes an empty and useless CRI object.
  // you need to call CodeRegionInfo::init()
  CodeRegionInfo() {
    UnknownFI = new FunctionInfo(FunctionDefinition(UnknownFn, false, 0, 0));
  }

  ~CodeRegionInfo() {
    // All function infos _except_ the unknown FI are in the AddrMap, since it
    // has no addr.

    // FIXME: an AddrMap can now have two intervals that point to the
    // same FunctionInfo, so this will cause a double-free.
    // two solutions come to mind:
    //
    // 1. (ugly) separately track which FunctionInfos we've already freed
    // and/or allocated to avoid double-freeing.
    //
    // 2. Use shared_ptr everywhere as much as possible. I know that ICL
    // can't handle shared_ptr, so in that case we can just grab the raw
    // pointer and pass it into the interval map.
    // the destructor for the interval map can safely ignore the shared_ptr
    // ctor. Anywhere we return a FunctionInfo*, we need to change it to a shared_ptr
    // outside of this interface.

    // for (auto Pair : AddrMap) {
    //   FunctionInfo *FI = Pair.second;
    //   delete FI;
    // }
    // delete UnknownFI;
  }
};

} // end halo namespace
