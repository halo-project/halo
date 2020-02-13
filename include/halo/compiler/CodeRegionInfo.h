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

class FunctionInfo {
public:
  FunctionInfo(std::string name, uint64_t start, uint64_t end, bool patchable)
    : Name(name), Patchable(patchable), Start(start), End(end) {}

  // the function's label / name.
  // generally the same across all clients
  std::string const& getName() const { return Name; }

  // whether this client can patch this function.
  // this fact is generally the same across all clients.
  bool isPatchable() const { return Patchable; }

  // real starting address in the process
  uint64_t getStart() const { return Start; }

  // real ending address in the process
  uint64_t getEnd() const { return End; }

  void dump(llvm::raw_ostream &out) const {
    out << "name = " << getName()
        << ", patchable = " << isPatchable()
        << ", start = " << getStart()
        << ", end = " << getEnd()
        << "\n";
  }

private:
  // These members are generally IDENTICAL across all clients
  std::string Name;
  bool Patchable;

  // These members always VARY across clients
  uint64_t Start;
  uint64_t End;
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

public:
  // fixed name for the unknown function
  static const std::string UnknownFn;

  // A special category for unknown functions. The FunctionInfo for this
  // "function" is returned on lookup failure.
  FunctionInfo *UnknownFI;

  // performs the actual initialization of the CRI based on the client enrollment
  void init(pb::ClientEnroll const& CE);

  // Upon failure to lookup information for the given IP or Name, the
  // UnknownFI is returned.
  FunctionInfo* lookup(uint64_t IP) const;
  FunctionInfo* lookup(std::string const& Name) const;
  void addRegion(std::string Name, uint64_t Start, uint64_t End, bool Patchable);

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

  // initializes an empty and useless CRI. you should use ::init()
  CodeRegionInfo() {
    UnknownFI = new FunctionInfo(UnknownFn, 0, 0, false);
  }

  ~CodeRegionInfo() {
    // All function infos _except_ the unknown FI are in the AddrMap, since it
    // has no addr.
    for (auto Pair : AddrMap) {
      FunctionInfo *FI = Pair.second;
      delete FI;
    }
    delete UnknownFI;
  }
};

} // end halo namespace
