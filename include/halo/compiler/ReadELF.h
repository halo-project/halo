#pragma once

#include "llvm/Object/ELFObjectFile.h"

#include <elf.h>

#include "Messages.pb.h"
#include "Logging.h"

namespace halo {

  /// reads symbol information from the generated dylib prior to sending it to clients.
  llvm::Error readSymbolInfo(llvm::MemoryBufferRef MBR, pb::LoadDyLib &Msg, std::string const& ExternalFn) {
    auto ExpectedObj = llvm::object::ObjectFile::createELFObjectFile(MBR);
    if (!ExpectedObj)
      return ExpectedObj.takeError();

    auto Obj = std::move(ExpectedObj.get());

    // gather symbol info from the dylib
    llvm::object::ELFObjectFileBase* ELF = llvm::dyn_cast_or_null<llvm::object::ELFObjectFileBase>(Obj.get());
    if (ELF == nullptr)
      return makeError("Only ELF object files are currently supported by Halo Monitor.");

    pb::LibFunctionSymbol *FS = nullptr;
    bool OneJITVisible = false;
    for (const llvm::object::ELFSymbolRef &Symb : ELF->symbols()) {
      auto MaybeType = Symb.getType();

      if (!MaybeType || MaybeType.get() != llvm::object::SymbolRef::Type::ST_Function)
        continue;

      auto MaybeName = Symb.getName();
      if (!MaybeName)
        continue;

      auto Name = MaybeName.get();

      auto ELFBinding = Symb.getBinding();
      bool ELFVisible = (ELFBinding == STB_GLOBAL) || (ELFBinding == STB_WEAK);

      // All function symbols in the ELF are expected to be externally visible.
      // This is needed because the JIT dynamic linker respects the visibility of
      // the object file. However, we need to know where in memory the linker has
      // placed all functions, whether it's private or not, for profiling purposes.
      // We can only get the address of symbols that are global from the dynamic linker.
      //
      // Thus, the only way get around that in the Halo monitor is to mark (after optimization)
      // all function symbols as external visibility, but continue treating the symbol like it's private
      // by not patching such functions, etc. That's what ExposeSymbolTablePass does.
      //
      // So, jit-visible means it's got a standard calling convention and can participate in code patching.
      //
      assert(ELFVisible && "Did you run ExposeSymbolTablePass prior to compiling?");

      bool JITVisible = (Name == ExternalFn);
      OneJITVisible = OneJITVisible || JITVisible;

      // logs() << "Symb: " << Symb.getELFTypeName()
      //       << ", " << Name
      //       << ", elf-visible = " << ELFVisible
      //       << ", jit-visible = " << JITVisible << "\n";

      FS = Msg.add_symbols();
      FS->set_label(Name.str());
      FS->set_externally_visible(JITVisible);
    }

    if (!OneJITVisible)
      return makeError("no symbols were marked as JIT-visible in this LoadDylib message.");

    return llvm::Error::success();
  }

} // end namespace