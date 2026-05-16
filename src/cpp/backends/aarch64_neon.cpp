#include "native.hpp"
#include "module_bindings.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string_view>

#include <arm_neon.h>

namespace nb = nanobind;

namespace massive_speedup::aarch64_neon {

namespace {

inline std::uint16_t to_bit_mask(uint8x16_t comparison) noexcept {
  static const uint8x16_t kBitMasks = {
      0x01u, 0x02u, 0x04u, 0x08u, 0x10u, 0x20u, 0x40u, 0x80u,
      0x01u, 0x02u, 0x04u, 0x08u, 0x10u, 0x20u, 0x40u, 0x80u,
  };
  const uint8x16_t masked = vandq_u8(comparison, kBitMasks);
  const std::uint16_t low = vaddv_u8(vget_low_u8(masked));
  const std::uint16_t high = vaddv_u8(vget_high_u8(masked));
  return static_cast<std::uint16_t>((high << 8) | low);
}

}  // namespace

struct NeonSpecialization : native::NativeSpecialization {
  template <std::size_t FieldCount>
  static void prescan_row(
      std::string_view line,
      std::array<std::uint16_t, native::detail::CsvLineCursor::kMaxFieldSlots>& offsets) {
    const uint8x16_t v_comma = vdupq_n_u8(static_cast<std::uint8_t>(','));
    const uint8x16_t v_quote = vdupq_n_u8(static_cast<std::uint8_t>('"'));

    offsets[0] = 0;
    std::size_t field = 1;
    std::uint32_t quote_carry = 0;

    const char* const data = line.data();
    const std::size_t n = line.size();
    alignas(16) std::uint8_t buffer[16];

    for (std::size_t off = 0; off < n; off += 16) {
      const std::size_t chunk_bytes = (n - off) < 16 ? (n - off) : 16;
      uint8x16_t bytes;
      if (chunk_bytes == 16) {
        bytes = vld1q_u8(reinterpret_cast<const std::uint8_t*>(data + off));
      } else {
        std::memset(buffer, 0, sizeof(buffer));
        std::memcpy(buffer, data + off, chunk_bytes);
        bytes = vld1q_u8(buffer);
      }

      const uint8x16_t cm_v = vceqq_u8(bytes, v_comma);
      const uint8x16_t qm_v = vceqq_u8(bytes, v_quote);
      const std::uint16_t valid = (chunk_bytes == 16)
          ? static_cast<std::uint16_t>(0xFFFFu)
          : static_cast<std::uint16_t>((1u << chunk_bytes) - 1u);
      const std::uint16_t cm = to_bit_mask(cm_v) & valid;
      const std::uint16_t qm = to_bit_mask(qm_v) & valid;

      std::uint32_t inside = qm;
      inside ^= inside << 1;
      inside ^= inside << 2;
      inside ^= inside << 4;
      inside ^= inside << 8;
      inside &= 0xFFFFu;
      inside ^= (quote_carry ? 0xFFFFu : 0u);

      quote_carry = (inside >> (chunk_bytes - 1)) & 1u;

      std::uint32_t delim = (static_cast<std::uint32_t>(cm) & ~inside) & 0xFFFFu;
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

}  // namespace massive_speedup::aarch64_neon

namespace massive_speedup::native {

template <typename Base>
using NeonImplementation = Implementation<Base, aarch64_neon::NeonSpecialization>;

}  // namespace massive_speedup::native

NB_MODULE(_neon, m) {
  massive_speedup::bindings::bind_native_module<massive_speedup::native::NeonImplementation>(
      m,
      "_Neon");
}
