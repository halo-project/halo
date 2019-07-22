#pragma once

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

} // namespace msg
} // namespace halo
