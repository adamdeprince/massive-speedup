#include "native.hpp"
#include "module_bindings.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string_view>

namespace nb = nanobind;

namespace massive_speedup::swar {

namespace {

constexpr std::uint64_t kByteOnes = 0x0101010101010101ULL;
constexpr std::uint64_t kByteHi = 0x8080808080808080ULL;

inline std::uint64_t broadcast_byte(std::uint8_t value) noexcept {
  return kByteOnes * static_cast<std::uint64_t>(value);
}

// Mycroft's haszero: bit 0x80 is set in each byte that was 0.
inline std::uint64_t haszero(std::uint64_t v) noexcept {
  return (v - kByteOnes) & ~v & kByteHi;
}

}  // namespace

struct SwarSpecialization : native::NativeSpecialization {
  template <std::size_t FieldCount>
  static void prescan_row(
      std::string_view line,
      std::array<std::uint16_t, native::detail::CsvLineCursor::kMaxFieldSlots>& offsets) {
    const std::uint64_t comma_word = broadcast_byte(static_cast<std::uint8_t>(','));
    const std::uint64_t quote_word = broadcast_byte(static_cast<std::uint8_t>('"'));

    offsets[0] = 0;
    std::size_t field = 1;
    std::size_t off = 0;
    std::uint32_t in_quote = 0;

    const char* const data = line.data();
    const std::size_t n = line.size();

    auto process_chunk = [&](std::uint64_t word, std::size_t base, std::size_t chunk_size) {
      const std::uint64_t cm = haszero(word ^ comma_word);
      const std::uint64_t qm = haszero(word ^ quote_word);
      std::uint64_t events = cm | qm;
      while (events) {
        const std::uint32_t bit = static_cast<std::uint32_t>(__builtin_ctzll(events));
        events &= events - 1;
        const std::uint32_t byte_pos = bit >> 3;
        if (byte_pos >= chunk_size) {
          continue;
        }
        const std::uint8_t character = static_cast<std::uint8_t>(word >> (byte_pos * 8));
        if (character == '"') {
          in_quote ^= 1u;
        } else {
          if (in_quote == 0u) {
            if (field >= FieldCount) {
              throw std::invalid_argument("too many fields in CSV row");
            }
            offsets[field++] = static_cast<std::uint16_t>(base + byte_pos + 1);
          }
        }
      }
    };

    for (; off + 8 <= n; off += 8) {
      std::uint64_t word;
      std::memcpy(&word, data + off, 8);
      process_chunk(word, off, 8);
    }

    if (off < n) {
      std::uint64_t word = 0;
      std::memcpy(&word, data + off, n - off);
      process_chunk(word, off, n - off);
    }

    if (field != FieldCount) {
      throw std::invalid_argument("too few fields in CSV row");
    }
    offsets[FieldCount] = static_cast<std::uint16_t>(n + 1);
  }
};

}  // namespace massive_speedup::swar

namespace massive_speedup::native {

template <typename Base>
using SwarImplementation = Implementation<Base, swar::SwarSpecialization>;

}  // namespace massive_speedup::native

NB_MODULE(_swar, m) {
  massive_speedup::bindings::bind_native_module<massive_speedup::native::SwarImplementation>(
      m,
      "_Swar");
}
