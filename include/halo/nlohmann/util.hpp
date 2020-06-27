#pragma once

#include "halo/nlohmann/json.hpp"
#include <string>

#include "Logging.h"

namespace halo {
  namespace config {

    inline void parseError(std::string const& hint) {
      fatal_error("Error during parsing of config file:\n\t" + hint);
    }

    inline bool contains(std::string const& Key, nlohmann::json const& Object) {
      return Object.find(Key) != Object.end();
    }

    // checked lookup of a value for the given key.
    // NOTE: key goes first, then json object!
    template <typename T>
    T getValue(std::string const& Key, nlohmann::json const& Root, std::string Context = "") {
      if (Context != "")
        Context = "(" + Context + ") ";

      if (!Root.is_object())
        parseError(Context + "expected an object.");

      if (!contains(Key, Root))
        parseError(Context + "expected member '" + Key + "' not found.");

       nlohmann::json const& Obj = Root[Key];

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

    // lookup of a value from the serverSettings in the root of the server config.
    // NOTE: key goes first, then json object!
    template <typename T>
    T getServerSetting(std::string const& Key, nlohmann::json const& Config) {
      const std::string SETTINGS = "serverSettings";
      if (!Config.is_object() || !contains(SETTINGS, Config))
        parseError("invalid / missing " + SETTINGS + " in top level of server config!");

       nlohmann::json const& Settings = Config[SETTINGS];

      return getValue<T>(Key, Settings, SETTINGS);
    }

    inline void setServerSetting(std::string const& Key, std::string const& Val, nlohmann::json &Config) {
      const std::string SETTINGS = "serverSettings";
      if (!Config.is_object() || !contains(SETTINGS, Config))
        parseError("invalid / missing " + SETTINGS + " in top level of server config!");

       nlohmann::json &Settings = Config[SETTINGS];

       Settings[Key] = Val;
    }

  } // end namespace config
} // end namespace halo