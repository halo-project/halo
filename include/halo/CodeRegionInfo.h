#pragma once

#include <vector>
#include <map>

#include "llvm/ADT/Optional.h"
// #include "llvm/IR/Module.h"

// Function interface reference:
// https://www.boost.org/doc/libs/1_65_0/libs/icl/doc/html/boost_icl/interface/function_synopsis.html

// [NOTE identity element]
// https://www.boost.org/doc/libs/1_65_0/libs/icl/doc/html/boost_icl/concepts/map_traits.html#boost_icl.concepts.map_traits.definedness_and_storage_of_identity_elements

#define BOOST_ICL_USE_STATIC_BOUNDED_INTERVALS
#include "boost/icl/interval_map.hpp"

#include "Messages.pb.h"

namespace icl = boost::icl;

namespace halo {

class Profiler;


struct FunctionInfo {
    std::string Name;

    // Ideally this would also provide block info.

    FunctionInfo(std::string name) : Name(name) {}
};

class CodeRegionInfo {
private:
  using CodeMap = icl::interval_map<uint64_t, FunctionInfo*,
                                    icl::partial_enricher>;
  friend class Profiler;

  CodeMap AddrMap;
  uint64_t VMABase;

  void init(pb::ClientEnroll const& CE);

public:

  llvm::Optional<FunctionInfo*> lookup(uint64_t IP) const;

  ~CodeRegionInfo() {
    // unfortunately, we can't use unique_ptr as an element type in this map
    // so we have to use new/delete.
    for (auto Pair : AddrMap) {
      FunctionInfo *FI = Pair.second;
      delete FI;
    }
  }


  // NOTE: possible double-free if Context outlives Module.
  // std::unique_ptr<llvm::LLVMContext> Cxt;
  // std::unique_ptr<llvm::Module> Module;
  //
  // CodeSectionInfo () : Cxt(new llvm::LLVMContext()) {}
  //
  // void dumpModule() const { Module->print(llvm::errs(), nullptr); }
};


/* NOTE: currently assuming only one code region. below is old infrastructure
         that had multiple disjoint code regions in mind (e.g., handle shared
        libs with bitcode). Not currently a priority.
class CodeRegionInfo {
public:

  CodeRegionInfo() {}

  ~CodeRegionInfo() {}

  // llvm::Optional<pb::FunctionInfo*> lookup(uint64_t IP) const;

  // void dumpModules() const {
  //   for (const auto &CSI : Data)
  //     CSI.dumpModule();
  // }

private:
  // interval map FROM code address offset TO function information

  // NOTE:
  // 1. it seems impossible to use a unique_ptr here because
  //    of the interface of interval_map's find().
  // 2. partial_enricher ensures that the map doesn't stupidly ignore
  //    inserts of the identity elem in co-domain, e.g., the pair {0,0}.
  //    see [NOTE identity element] link.


  // map FROM object filename TO code-section vector.
  std::map<std::string, uint64_t> ObjFiles;

  // interval map FROM this process's virtual-memory code addresses
  //              TO <an index of the code-section vector>.
  icl::interval_map<uint64_t, size_t, icl::partial_enricher> VMAResolver;

  // the code-section vector, which is paired with the offset to apply
  // to the raw IP to index into it.
  std::vector<CodeSectionInfo> Data;

};
*/

} // end halo namespace
