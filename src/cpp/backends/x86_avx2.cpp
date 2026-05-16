#include "native.hpp"
#include "module_bindings.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string_view>

#include <immintrin.h>

namespace nb = nanobind;

namespace massive_speedup::x86_avx2 {

struct Avx2Specialization : native::NativeSpecialization {
  template <std::size_t FieldCount>
  static void prescan_row(
      std::string_view line,
      std::array<std::uint16_t, native::detail::CsvLineCursor::kMaxFieldSlots>& offsets) {
    const __m256i v_comma = _mm256_set1_epi8(',');
    const __m256i v_quote = _mm256_set1_epi8('"');

    offsets[0] = 0;
    std::size_t field = 1;
    std::uint32_t quote_carry = 0;

    const char* const data = line.data();
    const std::size_t n = line.size();
    alignas(32) char buffer[32];

    for (std::size_t off = 0; off < n; off += 32) {
      const std::size_t chunk_bytes = (n - off) < 32 ? (n - off) : 32;
      __m256i bytes;
      if (chunk_bytes == 32) {
        bytes = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + off));
      } else {
        std::memset(buffer, 0, 32);
        std::memcpy(buffer, data + off, chunk_bytes);
        bytes = _mm256_load_si256(reinterpret_cast<const __m256i*>(buffer));
      }

      const std::uint32_t valid = (chunk_bytes == 32)
          ? 0xFFFFFFFFu
          : ((1u << chunk_bytes) - 1u);
      const std::uint32_t cm = static_cast<std::uint32_t>(
          _mm256_movemask_epi8(_mm256_cmpeq_epi8(bytes, v_comma))) & valid;
      const std::uint32_t qm = static_cast<std::uint32_t>(
          _mm256_movemask_epi8(_mm256_cmpeq_epi8(bytes, v_quote))) & valid;

      // Inclusive prefix-XOR of qm: bit i becomes parity of qm[0..i].
      std::uint32_t inside = qm;
      inside ^= inside << 1;
      inside ^= inside << 2;
      inside ^= inside << 4;
      inside ^= inside << 8;
      inside ^= inside << 16;
      inside ^= static_cast<std::uint32_t>(-static_cast<std::int32_t>(quote_carry));

      // Update carry: parity at end of valid bytes.
      quote_carry = (inside >> (chunk_bytes - 1)) & 1u;

      std::uint32_t delim = cm & ~inside;
      while (delim) {
        const std::uint32_t bit = static_cast<std::uint32_t>(__builtin_ctz(delim));
        if (field >= FieldCount) {
          throw std::invalid_argument("too many fields in CSV row");
        }
        offsets[field++] = static_cast<std::uint16_t>(off + bit + 1);
        delim &= delim - 1;
      }
    }

    if (field != FieldCount) {
      throw std::invalid_argument("too few fields in CSV row");
    }
    offsets[FieldCount] = static_cast<std::uint16_t>(n + 1);
  }
};

}  // namespace massive_speedup::x86_avx2

namespace massive_speedup::native {

template <typename Base>
using Avx2Implementation = Implementation<Base, x86_avx2::Avx2Specialization>;

}  // namespace massive_speedup::native

NB_MODULE(_avx2, m) {
  massive_speedup::bindings::bind_native_module<massive_speedup::native::Avx2Implementation>(
      m,
      "_Avx2");
}
