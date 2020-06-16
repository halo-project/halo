#pragma once

#include "halo/nlohmann/json.hpp"
#include <string>

#include "Logging.h"

namespace halo {
  namespace config {

    inline void parseError(std::string const& hint) {
      fatal_error("Error during parsing of config file:\n\t" + hint);
    }

    inline bool contains(std::string const& Key, JSON const& Object) {
      return Object.find(Key) != Object.end();
    }

    // checked lookup of a value for the given key.
    template <typename T>
    T getValue(std::string const& Key, nlohmann::json const& Root, std::string Context = "") {
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

  } // end namespace config
} // end namespace halo