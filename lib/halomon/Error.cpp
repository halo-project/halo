
#include <iostream>
#include <cstdlib>

namespace boost {

void throw_exception(std::exception const& ex) {
  std::cerr << "uncaught exception: " << ex.what() << "\n";
  std::exit(EXIT_FAILURE);
}

}

namespace halo {
  void fatal_error(const std::string &msg) {
    std::cerr << "fatal error: " << msg << "\n";
    std::exit(EXIT_FAILURE);
  }
}
