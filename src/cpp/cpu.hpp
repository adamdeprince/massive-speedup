#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace massive_speedup {

enum class BackendKind : std::uint8_t {
  generic = 0,
  swar = 1,
  x86_avx2 = 2,
  x86_avx512bw = 3,
  aarch64_neon = 4,
};

struct BackendRecord {
  BackendKind kind;
  std::string name;
  bool compiled;
  bool available;
};

std::string_view backend_name(BackendKind kind) noexcept;
bool backend_is_compiled(BackendKind kind) noexcept;
bool backend_is_available(BackendKind kind) noexcept;
BackendKind detect_best_backend() noexcept;
std::vector<BackendRecord> available_backends();

}  // namespace massive_speedup
