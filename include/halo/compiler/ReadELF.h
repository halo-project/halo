#pragma once

#include "llvm/Object/ELFObjectFile.h"

#include <elf.h>

#include "Messages.pb.h"
#include "Logging.h"

namespace halo {

  llvm::Error readSymbolInfo(llvm::MemoryBufferRef MBR, pb::LoadDyLib &Msg) {
    auto ExpectedObj = llvm::object::ObjectFile::createELFObjectFile(MBR);
    if (!ExpectedObj)
      return ExpectedObj.takeError();

    auto Obj = std::move(ExpectedObj.get());

    // gather symbol info from the dylib
    llvm::object::ELFObjectFileBase* ELF = llvm::dyn_cast_or_null<llvm::object::ELFObjectFileBase>(Obj.get());
    if (ELF == nullptr)
      return makeError("Only ELF object files are currently supported by Halo Monitor.");

    pb::LibFunctionSymbol *FS = nullptr;
    for (const llvm::object::ELFSymbolRef &Symb : ELF->symbols()) {
      auto MaybeType = Symb.getType();

      if (!MaybeType || MaybeType.get() != llvm::object::SymbolRef::Type::ST_Function)
        continue;

      auto MaybeName = Symb.getName();
      if (!MaybeName)
        continue;

      auto Name = MaybeName.get();

      auto ELFBinding = Symb.getBinding();
      bool Visible = (ELFBinding == STB_GLOBAL) || (ELFBinding == STB_WEAK);

      logs() << "Symb: " << Symb.getELFTypeName() << ", " << Name << ", visible = " << Visible << "\n";

      FS = Msg.add_symbols();
      FS->set_label(Name.str());
      FS->set_externally_visible(Visible);
    }
    return llvm::Error::success();
  }

} // end namespace