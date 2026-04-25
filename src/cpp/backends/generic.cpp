#include "backends/generic.hpp"
#include "module_bindings.hpp"

namespace nb = nanobind;

NB_MODULE(_generic, m) {
  massive_speedup::bindings::bind_backend_module<massive_speedup::backend_generic::Implementation>(
      m,
      "_Generic");
}
