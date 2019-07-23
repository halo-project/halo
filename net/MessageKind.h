#pragma once

#include <iostream>

#include "google/protobuf/util/json_util.h"

namespace proto = google::protobuf;


namespace halo {
  namespace msg {
    // NOTE: changing any of the existing numbers here breaks backwards compatibility.
    typedef enum Kind_{
      None = 0,
      ClientEnroll = 1,
      RawSample = 2,
      StartSampling = 3,
      StopSampling = 4
    } Kind;

    template<typename T> // type param here mainly to prevent multiple redefinition.
    T kind_to_str(Kind K) {
      switch (K) {
        case None: return "None";
        case ClientEnroll: return "ClientEnroll";
        case RawSample: return "RawSample";
        case StartSampling: return "StartSampling";
        case StopSampling: return "StopSampling";
        default: return "<unknown>";
      }
    }

    template<typename T>
    void print_proto(T &Proto) {
      std::string AsJSON;
      proto::util::JsonPrintOptions Opts;
      Opts.add_whitespace = true;
      proto::util::MessageToJsonString(Proto, &AsJSON, Opts);
      std::cerr << AsJSON << "\n---\n";
    }

  } // namespace msg
} // namespace halo
