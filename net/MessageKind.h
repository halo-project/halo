#pragma once

#include <iostream>
#include "Logging.h"

#include "google/protobuf/util/json_util.h"

namespace proto = google::protobuf;


namespace halo {
  namespace msg {
    // NOTE: changing any of the existing numbers here breaks backwards compatibility.
    typedef enum Kind_{
      None = 0, // an invalid message kind to detect default-value errors.
      ClientEnroll = 1,
      RawSample = 2,
      StartSampling = 3,
      StopSampling = 4,
      SetSamplingPeriod = 5,
      Shutdown = 6, // Not actually sent by anybody. it's "recieved" when a connection closes or hit an error.
      LoadDyLib = 7,
      BakeoffResult = 8,
      ModifyFunction = 9,
      FunctionMeasurements = 10,
      DyLibInfo = 11

    } Kind;

    template<typename T> // type param here mainly to prevent multiple redefinition.
    T kind_to_str(Kind K) {
      switch (K) {
        case None: return "None";
        case ClientEnroll: return "ClientEnroll";
        case RawSample: return "RawSample";
        case StartSampling: return "StartSampling";
        case StopSampling: return "StopSampling";
        case SetSamplingPeriod: return "SetSamplingPeriod";
        case Shutdown: return "Shutdown";
        case LoadDyLib: return "LoadDyLib";
        case BakeoffResult: return "BakeoffResult";
        case ModifyFunction: return "ModifyFunction";
        case FunctionMeasurements: return "FunctionMeasurements";
        case DyLibInfo: return "DyLibInfo";
        default: return "<unknown>";
      }
    }

    template<typename T>
    void print_proto(T &Proto) {
      std::string AsJSON;
      proto::util::JsonPrintOptions Opts;
      Opts.add_whitespace = true;
      proto::util::MessageToJsonString(Proto, &AsJSON, Opts);
      logs() << AsJSON << "\n---\n";
    }

  } // namespace msg
} // namespace halo
