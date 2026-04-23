#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace massive_speedup {

enum class BackendKind {
  generic = 0,
  x86_sse42,
  x86_avx2,
  x86_avx512,
  linux_aarch64_neon,
  linux_aarch64_sve,
  linux_aarch64_sve2,
  linux_loongarch64_lsx,
  linux_loongarch64_lasx,
};

struct BackendRecord {
  BackendKind kind;
  const char* name;
  bool compiled;
  bool available;
};

constexpr std::string_view backend_kind_name(BackendKind kind) noexcept {
  switch (kind) {
    case BackendKind::generic:
      return "generic";
    case BackendKind::x86_sse42:
      return "x86_sse42";
    case BackendKind::x86_avx2:
      return "x86_avx2";
    case BackendKind::x86_avx512:
      return "x86_avx512";
    case BackendKind::linux_aarch64_neon:
      return "linux_aarch64_neon";
    case BackendKind::linux_aarch64_sve:
      return "linux_aarch64_sve";
    case BackendKind::linux_aarch64_sve2:
      return "linux_aarch64_sve2";
    case BackendKind::linux_loongarch64_lsx:
      return "linux_loongarch64_lsx";
    case BackendKind::linux_loongarch64_lasx:
      return "linux_loongarch64_lasx";
  }

  return "unknown";
}

BackendKind detect_best_backend() noexcept;
BackendKind module_backend_kind() noexcept;
bool backend_is_available(BackendKind kind) noexcept;
std::vector<BackendRecord> available_backends();

enum class ProcessorType {
  Generic = 0,
  Sse42 = 1,
  Avx = 2,
  Avx512Bw = 3,
  Neon = 4,
  Sve = 5,
  Sve2 = 6,
  Lsx = 7,
  Lasx = 8,
};

ProcessorType detect_processor_type();
ProcessorType module_processor_type();
std::string processor_type_name(ProcessorType processor_type);

}  // namespace massive_speedup
