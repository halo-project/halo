
#include "halo/Profiler.h"

// #include "clang/9.0.0/"

#include <iostream>

namespace object = llvm::object;

namespace halo {

void Profiler::recordData1(IDType ID, DataKind DK, uint64_t Val) {
  switch (DK) {
    case DataKind::InstrPtr: {

      // TODO: use compiler-rt's symbolizer.


      /*
      // TODO: this needs to be relative to the start of code section, not a VMA.
      uint64_t Offset = Val;

      auto ResOrErr = Symbolizer->symbolizeCode(
          BinaryPath, {Offset, object::SectionedAddress::UndefSection});

      if (!ResOrErr) {
        std::cerr << "Error in symbolization\n";
        return;
      }

      auto DILineInfo = ResOrErr.get();

      std::cerr << "IP = " << std::hex << Val << " --> " << DILineInfo.FunctionName << "\n";
      */

    } break;

  }
}

// void Profiler::recordData2(IDType ID, uint64_t Val1, uint64_t Val2) {
//   // todo.
// }

}
