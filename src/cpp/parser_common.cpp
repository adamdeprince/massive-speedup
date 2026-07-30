#include "massive_speedup/parsers.hpp"

namespace massive_speedup {

std::string Parser::serialize() const {
  return "parser_group=" + parser_group() + ";asset_class=" + asset_class() +
         ";processor=" + processor_name();
}

std::string Parser::processor_name() const { return "generic"; }

}  // namespace massive_speedup
