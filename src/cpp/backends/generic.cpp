#include "native.hpp"
#include "module_bindings.hpp"

namespace nb = nanobind;

NB_MODULE(_generic, m) {
  massive_speedup::bindings::bind_native_module<massive_speedup::native::Implementation>(
      m,
      "_Generic");
}
