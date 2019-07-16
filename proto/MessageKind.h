#pragma once

namespace halo {

  namespace msg {

// NOTE: changing any of the existing numbers here breaks backwards compatibility.
typedef enum Kind_{
  None = 0,
  RawSample = 1
} Kind;

} // namespace msg
} // namespace halo
