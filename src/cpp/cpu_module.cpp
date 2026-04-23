#include <nanobind/nanobind.h>
#include <nanobind/stl/vector.h>

#include "massive_speedup/cpu.hpp"

namespace nb = nanobind;
using namespace massive_speedup;

NB_MODULE(_cpu, m) {
  nb::enum_<BackendKind>(m, "BackendKind")
      .value("GENERIC", BackendKind::generic)
      .value("X86_SSE42", BackendKind::x86_sse42)
      .value("X86_AVX2", BackendKind::x86_avx2)
      .value("X86_AVX512", BackendKind::x86_avx512)
      .value("LINUX_AARCH64_NEON", BackendKind::linux_aarch64_neon)
      .value("LINUX_AARCH64_SVE", BackendKind::linux_aarch64_sve)
      .value("LINUX_AARCH64_SVE2", BackendKind::linux_aarch64_sve2)
      .value("LINUX_LOONGARCH64_LSX", BackendKind::linux_loongarch64_lsx)
      .value("LINUX_LOONGARCH64_LASX", BackendKind::linux_loongarch64_lasx);

  nb::enum_<ProcessorType>(m, "ProcessorType")
      .value("GENERIC", ProcessorType::Generic)
      .value("SSE42", ProcessorType::Sse42)
      .value("AVX", ProcessorType::Avx)
      .value("AVX512BW", ProcessorType::Avx512Bw)
      .value("NEON", ProcessorType::Neon)
      .value("SVE", ProcessorType::Sve)
      .value("SVE2", ProcessorType::Sve2)
      .value("LSX", ProcessorType::Lsx)
      .value("LASX", ProcessorType::Lasx);

  nb::class_<BackendRecord>(m, "BackendRecord")
      .def_ro("kind", &BackendRecord::kind)
      .def_ro("name", &BackendRecord::name)
      .def_ro("compiled", &BackendRecord::compiled)
      .def_ro("available", &BackendRecord::available);

  m.def("detect_best_backend", &detect_best_backend);
  m.def("backend_is_available", &backend_is_available, nb::arg("kind"));
  m.def("available_backends", &available_backends);
  m.def("detect_processor_type", &detect_processor_type);
}
