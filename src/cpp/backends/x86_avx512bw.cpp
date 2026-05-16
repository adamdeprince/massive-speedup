#include "native.hpp"
#include "module_bindings.hpp"

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string_view>

#include <immintrin.h>

namespace nb = nanobind;

namespace massive_speedup::x86_avx512bw {

struct Avx512BwSpecialization : native::NativeSpecialization {
  template <std::size_t FieldCount>
  static void prescan_row(
      std::string_view line,
      std::array<std::uint16_t, native::detail::CsvLineCursor::kMaxFieldSlots>& offsets) {
    const __m512i v_comma = _mm512_set1_epi8(',');
    const __m512i v_quote = _mm512_set1_epi8('"');

    offsets[0] = 0;
    std::size_t field = 1;
    std::uint64_t quote_carry = 0;

    const char* const data = line.data();
    const std::size_t n = line.size();

    for (std::size_t off = 0; off < n; off += 64) {
      const std::size_t chunk_bytes = (n - off) < 64 ? (n - off) : 64;
      const __mmask64 load_mask = chunk_bytes == 64
          ? ~static_cast<__mmask64>(0)
          : ((static_cast<__mmask64>(1) << chunk_bytes) - 1);
      const __m512i bytes = _mm512_maskz_loadu_epi8(load_mask, data + off);

      // Compare-against gives a kmask; AND with load_mask drops bits past the chunk.
      const std::uint64_t cm = static_cast<std::uint64_t>(
          _mm512_cmpeq_epi8_mask(bytes, v_comma)) & static_cast<std::uint64_t>(load_mask);
      const std::uint64_t qm = static_cast<std::uint64_t>(
          _mm512_cmpeq_epi8_mask(bytes, v_quote)) & static_cast<std::uint64_t>(load_mask);

      // Inclusive prefix-XOR of qm: bit i becomes parity of qm[0..i].
      std::uint64_t inside = qm;
      inside ^= inside << 1;
      inside ^= inside << 2;
      inside ^= inside << 4;
      inside ^= inside << 8;
      inside ^= inside << 16;
      inside ^= inside << 32;
      inside ^= static_cast<std::uint64_t>(-static_cast<std::int64_t>(quote_carry));

      // Update carry to parity at end of valid bytes for next chunk.
      quote_carry = (inside >> (chunk_bytes - 1)) & 1ull;

      std::uint64_t delim = cm & ~inside;
      while (delim) {
        const std::uint32_t bit = static_cast<std::uint32_t>(__builtin_ctzll(delim));
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

}  // namespace massive_speedup::x86_avx512bw

namespace massive_speedup::native {

template <typename Base>
using Avx512BwImplementation = Implementation<Base, x86_avx512bw::Avx512BwSpecialization>;

}  // namespace massive_speedup::native

NB_MODULE(_avx512bw, m) {
  massive_speedup::bindings::bind_native_module<massive_speedup::native::Avx512BwImplementation>(
      m,
      "_Avx512Bw");
}
