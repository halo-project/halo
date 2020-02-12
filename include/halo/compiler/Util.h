#pragma once

#include <ostream>
#include <sstream>
#include <iomanip>

namespace halo {

/// a to_string method for floats that accepts a level
/// of precision to use, which is defined as 'the number of
/// digits to use to express the fractional part of the number'
template <typename FPType>
std::string to_formatted_str(FPType x, unsigned precision = 2) {
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(precision) << x;
  return stream.str();
}

}