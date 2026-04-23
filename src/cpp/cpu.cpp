#include "massive_speedup/cpu.hpp"

#include <array>

namespace massive_speedup {

namespace {

ProcessorType processor_type_from_backend_kind(BackendKind kind) noexcept {
  switch (kind) {
    case BackendKind::generic:
      return ProcessorType::Generic;
    case BackendKind::x86_sse42:
      return ProcessorType::Sse42;
    case BackendKind::x86_avx2:
      return ProcessorType::Avx;
    case BackendKind::x86_avx512:
      return ProcessorType::Avx512Bw;
    case BackendKind::linux_aarch64_neon:
      return ProcessorType::Neon;
    case BackendKind::linux_aarch64_sve:
      return ProcessorType::Sve;
    case BackendKind::linux_aarch64_sve2:
      return ProcessorType::Sve2;
    case BackendKind::linux_loongarch64_lsx:
      return ProcessorType::Lsx;
    case BackendKind::linux_loongarch64_lasx:
      return ProcessorType::Lasx;
  }

  return ProcessorType::Generic;
}

bool backend_is_compiled(BackendKind kind) noexcept {
  switch (kind) {
    case BackendKind::generic:
      return true;
    case BackendKind::x86_sse42:
#if defined(MASSIVE_SPEEDUP_HAVE_X86_SSE42)
      return true;
#else
      return false;
#endif
    case BackendKind::x86_avx2:
#if defined(MASSIVE_SPEEDUP_HAVE_X86_AVX2)
      return true;
#else
      return false;
#endif
    case BackendKind::x86_avx512:
#if defined(MASSIVE_SPEEDUP_HAVE_X86_AVX512)
      return true;
#else
      return false;
#endif
    case BackendKind::linux_aarch64_neon:
#if defined(MASSIVE_SPEEDUP_HAVE_LINUX_AARCH64_NEON)
      return true;
#else
      return false;
#endif
    case BackendKind::linux_aarch64_sve:
#if defined(MASSIVE_SPEEDUP_HAVE_LINUX_AARCH64_SVE)
      return true;
#else
      return false;
#endif
    case BackendKind::linux_aarch64_sve2:
#if defined(MASSIVE_SPEEDUP_HAVE_LINUX_AARCH64_SVE2)
      return true;
#else
      return false;
#endif
    case BackendKind::linux_loongarch64_lsx:
#if defined(MASSIVE_SPEEDUP_HAVE_LINUX_LOONGARCH64_LSX)
      return true;
#else
      return false;
#endif
    case BackendKind::linux_loongarch64_lasx:
#if defined(MASSIVE_SPEEDUP_HAVE_LINUX_LOONGARCH64_LASX)
      return true;
#else
      return false;
#endif
  }

  return false;
}

}  // namespace

BackendKind module_backend_kind() noexcept {
#if defined(MASSIVE_SPEEDUP_PROCESSOR_AVX512BW)
  return BackendKind::x86_avx512;
#elif defined(MASSIVE_SPEEDUP_PROCESSOR_AVX)
  return BackendKind::x86_avx2;
#elif defined(MASSIVE_SPEEDUP_PROCESSOR_SSE42)
  return BackendKind::x86_sse42;
#elif defined(MASSIVE_SPEEDUP_PROCESSOR_SVE2)
  return BackendKind::linux_aarch64_sve2;
#elif defined(MASSIVE_SPEEDUP_PROCESSOR_SVE)
  return BackendKind::linux_aarch64_sve;
#elif defined(MASSIVE_SPEEDUP_PROCESSOR_NEON)
  return BackendKind::linux_aarch64_neon;
#elif defined(MASSIVE_SPEEDUP_PROCESSOR_LASX)
  return BackendKind::linux_loongarch64_lasx;
#elif defined(MASSIVE_SPEEDUP_PROCESSOR_LSX)
  return BackendKind::linux_loongarch64_lsx;
#else
  return BackendKind::generic;
#endif
}

ProcessorType module_processor_type() {
  return processor_type_from_backend_kind(module_backend_kind());
}

std::string processor_type_name(ProcessorType processor_type) {
  switch (processor_type) {
    case ProcessorType::Generic:
      return "generic";
    case ProcessorType::Sse42:
      return "sse42";
    case ProcessorType::Avx:
      return "avx2";
    case ProcessorType::Avx512Bw:
      return "avx512";
    case ProcessorType::Neon:
      return "neon";
    case ProcessorType::Sve:
      return "sve";
    case ProcessorType::Sve2:
      return "sve2";
    case ProcessorType::Lsx:
      return "lsx";
    case ProcessorType::Lasx:
      return "lasx";
  }

  return "generic";
}

ProcessorType detect_processor_type() {
#if defined(__x86_64__) || defined(__i386__)
#if defined(__GNUC__) || defined(__clang__)
  if (__builtin_cpu_supports("avx512bw")) {
    return ProcessorType::Avx512Bw;
  }
  if (__builtin_cpu_supports("avx2")) {
    return ProcessorType::Avx;
  }
  if (__builtin_cpu_supports("sse4.2")) {
    return ProcessorType::Sse42;
  }
#endif
  return ProcessorType::Generic;
#elif defined(__aarch64__) || defined(__arm__)
#if defined(__ARM_FEATURE_SVE2)
  return ProcessorType::Sve2;
#elif defined(__ARM_FEATURE_SVE)
  return ProcessorType::Sve;
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
  return ProcessorType::Neon;
#else
  return ProcessorType::Generic;
#endif
#elif defined(__loongarch__)
#if defined(__loongarch_asx)
  return ProcessorType::Lasx;
#elif defined(__loongarch_sx)
  return ProcessorType::Lsx;
#else
  return ProcessorType::Generic;
#endif
#else
  return ProcessorType::Generic;
#endif
}

bool backend_is_available(BackendKind kind) noexcept {
  if (!backend_is_compiled(kind)) {
    return false;
  }

  switch (kind) {
    case BackendKind::generic:
      return true;
    case BackendKind::x86_sse42:
      return detect_processor_type() == ProcessorType::Sse42 ||
             detect_processor_type() == ProcessorType::Avx ||
             detect_processor_type() == ProcessorType::Avx512Bw;
    case BackendKind::x86_avx2:
      return detect_processor_type() == ProcessorType::Avx ||
             detect_processor_type() == ProcessorType::Avx512Bw;
    case BackendKind::x86_avx512:
      return detect_processor_type() == ProcessorType::Avx512Bw;
    case BackendKind::linux_aarch64_neon:
      return detect_processor_type() == ProcessorType::Neon ||
             detect_processor_type() == ProcessorType::Sve ||
             detect_processor_type() == ProcessorType::Sve2;
    case BackendKind::linux_aarch64_sve:
      return detect_processor_type() == ProcessorType::Sve ||
             detect_processor_type() == ProcessorType::Sve2;
    case BackendKind::linux_aarch64_sve2:
      return detect_processor_type() == ProcessorType::Sve2;
    case BackendKind::linux_loongarch64_lsx:
      return detect_processor_type() == ProcessorType::Lsx ||
             detect_processor_type() == ProcessorType::Lasx;
    case BackendKind::linux_loongarch64_lasx:
      return detect_processor_type() == ProcessorType::Lasx;
  }

  return false;
}

BackendKind detect_best_backend() noexcept {
#if defined(__x86_64__) || defined(__i386__)
  if (backend_is_available(BackendKind::x86_avx512)) return BackendKind::x86_avx512;
  if (backend_is_available(BackendKind::x86_avx2)) return BackendKind::x86_avx2;
  if (backend_is_available(BackendKind::x86_sse42)) return BackendKind::x86_sse42;
  return BackendKind::generic;
#elif defined(__linux__) && (defined(__aarch64__) || defined(__arm__))
  if (backend_is_available(BackendKind::linux_aarch64_sve2)) return BackendKind::linux_aarch64_sve2;
  if (backend_is_available(BackendKind::linux_aarch64_sve)) return BackendKind::linux_aarch64_sve;
  if (backend_is_available(BackendKind::linux_aarch64_neon)) return BackendKind::linux_aarch64_neon;
  return BackendKind::generic;
#elif defined(__loongarch__)
  if (backend_is_available(BackendKind::linux_loongarch64_lasx)) return BackendKind::linux_loongarch64_lasx;
  if (backend_is_available(BackendKind::linux_loongarch64_lsx)) return BackendKind::linux_loongarch64_lsx;
  return BackendKind::generic;
#else
  return BackendKind::generic;
#endif
}

std::vector<BackendRecord> available_backends() {
  const std::array<BackendKind, 9> all = {
      BackendKind::generic,
      BackendKind::x86_sse42,
      BackendKind::x86_avx2,
      BackendKind::x86_avx512,
      BackendKind::linux_aarch64_neon,
      BackendKind::linux_aarch64_sve,
      BackendKind::linux_aarch64_sve2,
      BackendKind::linux_loongarch64_lsx,
      BackendKind::linux_loongarch64_lasx,
  };

  std::vector<BackendRecord> out;
  out.reserve(all.size());
  for (const auto kind : all) {
    out.push_back(BackendRecord{
        kind,
        backend_kind_name(kind).data(),
        backend_is_compiled(kind),
        backend_is_available(kind),
    });
  }
  return out;
}

}  // namespace massive_speedup
