#include "cpu.hpp"

#include <nanobind/nanobind.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

namespace nb = nanobind;

NB_MODULE(_cpu, m) {
  m.doc() = "CPU backend detection for massive_speedup.";

  nb::enum_<massive_speedup::BackendKind>(m, "BackendKind")
      .value("GENERIC", massive_speedup::BackendKind::generic)
      .value("SWAR", massive_speedup::BackendKind::swar)
      .value("X86_AVX2", massive_speedup::BackendKind::x86_avx2)
      .value("X86_AVX512BW", massive_speedup::BackendKind::x86_avx512bw)
      .value("AARCH64_NEON", massive_speedup::BackendKind::aarch64_neon);

  nb::class_<massive_speedup::BackendRecord>(m, "BackendRecord")
      .def_ro("kind", &massive_speedup::BackendRecord::kind)
      .def_ro("name", &massive_speedup::BackendRecord::name)
      .def_ro("compiled", &massive_speedup::BackendRecord::compiled)
      .def_ro("available", &massive_speedup::BackendRecord::available);

  m.def("backend_name", [](massive_speedup::BackendKind kind) {
    return std::string(massive_speedup::backend_name(kind));
  });
  m.def("backend_is_compiled", &massive_speedup::backend_is_compiled);
  m.def("backend_is_available", &massive_speedup::backend_is_available);
  m.def("detect_best_backend", &massive_speedup::detect_best_backend);
  m.def("available_backends", &massive_speedup::available_backends);
}
