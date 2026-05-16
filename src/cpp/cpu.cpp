#include "cpu.hpp"

#include <array>

#if defined(__linux__) && defined(__aarch64__)
#include <sys/auxv.h>
#include <asm/hwcap.h>
#endif

namespace massive_speedup {

namespace {

struct BackendMetadata {
  BackendKind kind;
  const char* name;
};

constexpr std::array kBackends = {
    BackendMetadata{BackendKind::generic, "generic"},
    BackendMetadata{BackendKind::swar, "swar"},
    BackendMetadata{BackendKind::x86_avx2, "x86_avx2"},
    BackendMetadata{BackendKind::x86_avx512bw, "x86_avx512bw"},
    BackendMetadata{BackendKind::aarch64_neon, "aarch64_neon"},
};

bool supports_64_bit_swar() noexcept { return sizeof(void*) == 8; }

bool x86_supports_avx2() noexcept {
#if (defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)) && \
    (defined(__GNUC__) || defined(__clang__))
  __builtin_cpu_init();
  return __builtin_cpu_supports("avx2") != 0 && __builtin_cpu_supports("bmi") != 0 &&
      __builtin_cpu_supports("bmi2") != 0;
#else
  return false;
#endif
}

bool x86_supports_avx512bw() noexcept {
#if (defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)) && \
    (defined(__GNUC__) || defined(__clang__))
  __builtin_cpu_init();
  return __builtin_cpu_supports("avx512f") != 0 && __builtin_cpu_supports("avx512bw") != 0 &&
      __builtin_cpu_supports("avx512vl") != 0 && __builtin_cpu_supports("bmi2") != 0;
#else
  return false;
#endif
}

bool aarch64_supports_neon() noexcept {
#if defined(__aarch64__) || defined(_M_ARM64)
#if defined(__linux__)
  return (getauxval(AT_HWCAP) & HWCAP_ASIMD) != 0;
#else
  return true;
#endif
#else
  return false;
#endif
}

}  // namespace

std::string_view backend_name(BackendKind kind) noexcept {
  for (const auto& backend : kBackends) {
    if (backend.kind == kind) {
      return backend.name;
    }
  }
  return "unknown";
}

bool backend_is_compiled(BackendKind kind) noexcept {
  switch (kind) {
    case BackendKind::generic:
      return true;
    case BackendKind::swar:
#ifdef MASSIVE_SPEEDUP_HAVE_SWAR
      return true;
#else
      return false;
#endif
    case BackendKind::x86_avx2:
#ifdef MASSIVE_SPEEDUP_HAVE_X86_AVX2
      return true;
#else
      return false;
#endif
    case BackendKind::x86_avx512bw:
#ifdef MASSIVE_SPEEDUP_HAVE_X86_AVX512BW
      return true;
#else
      return false;
#endif
    case BackendKind::aarch64_neon:
#ifdef MASSIVE_SPEEDUP_HAVE_AARCH64_NEON
      return true;
#else
      return false;
#endif
  }
  return false;
}

bool backend_is_available(BackendKind kind) noexcept {
  if (!backend_is_compiled(kind)) {
    return false;
  }
  switch (kind) {
    case BackendKind::generic:
      return true;
    case BackendKind::swar:
      return supports_64_bit_swar();
    case BackendKind::x86_avx2:
      return x86_supports_avx2();
    case BackendKind::x86_avx512bw:
      return x86_supports_avx512bw();
    case BackendKind::aarch64_neon:
      return aarch64_supports_neon();
  }
  return false;
}

BackendKind detect_best_backend() noexcept {
  constexpr std::array priority = {
      BackendKind::x86_avx512bw,
      BackendKind::x86_avx2,
      BackendKind::aarch64_neon,
      BackendKind::swar,
      BackendKind::generic,
  };

  for (const auto kind : priority) {
    if (backend_is_available(kind)) {
      return kind;
    }
  }
  return BackendKind::generic;
}

std::vector<BackendRecord> available_backends() {
  std::vector<BackendRecord> records;
  records.reserve(kBackends.size());
  for (const auto& backend : kBackends) {
    records.push_back(BackendRecord{
        .kind = backend.kind,
        .name = std::string(backend.name),
        .compiled = backend_is_compiled(backend.kind),
        .available = backend_is_available(backend.kind),
    });
  }
  return records;
}

}  // namespace massive_speedup
