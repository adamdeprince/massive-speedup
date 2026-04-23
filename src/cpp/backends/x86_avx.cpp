#include "backends/generic.hpp"
#include "module_bindings.hpp"

#include <immintrin.h>

namespace nb = nanobind;

namespace massive_speedup::backend_x86_avx {

struct Specialization {
  static inline std::size_t find_byte_avx(
      std::string_view text,
      std::size_t start,
      char needle) {
    const std::size_t size = text.size();
    if (start >= size) {
      return std::string_view::npos;
    }

    const char* data = text.data();
    constexpr std::size_t chunk_size = 32;
    const __m256i target = _mm256_set1_epi8(needle);

    std::size_t index = start;
    while (size - index >= chunk_size) {
      const __m256i block =
          _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + index));
      const int mask = _mm256_movemask_epi8(_mm256_cmpeq_epi8(block, target));
      if (mask != 0) {
        return index + static_cast<std::size_t>(__builtin_ctz(mask));
      }

      index += chunk_size;
    }

    for (; index < size; ++index) {
      if (data[index] == needle) {
        return index;
      }
    }

    return std::string_view::npos;
  }

  static inline bool next_line(
      massive_speedup::backend_generic::detail::BufferedGzipLineReader& reader,
      std::string_view& line) {
    if (reader.line_start_ >= reader.pending_.size()) {
      reader.clear_consumed_buffer();
    }

    while (true) {
      const auto newline = reader.pending_.find('\n', reader.search_offset_);
      if (newline != std::string::npos) {
        line = reader.line_view(reader.line_start_, newline);
        reader.line_start_ = newline + 1;
        reader.search_offset_ = reader.line_start_;
        return true;
      }

      reader.search_offset_ = reader.pending_.size();
      if (!reader.read_more()) {
        break;
      }
    }

    if (reader.line_start_ < reader.pending_.size()) {
      line = reader.line_view(reader.line_start_, reader.pending_.size());
      reader.line_start_ = reader.pending_.size();
      reader.search_offset_ = reader.line_start_;
      return true;
    }

    line = {};
    return false;
  }

  template <bool ExpectMore>
  static inline std::string_view parse_unquoted_field(
      std::string_view line,
      std::size_t& cursor) {
    return massive_speedup::backend_generic::detail::CsvLineCursor::
        template scalar_parse_unquoted_field<ExpectMore>(line, cursor);
  }

  template <bool ExpectMore>
  static inline std::string_view parse_quoted_field(
      std::string_view line,
      std::size_t& cursor,
      std::string& scratch) {
    return massive_speedup::backend_generic::detail::CsvLineCursor::
        template scalar_parse_quoted_field<ExpectMore>(line, cursor, scratch);
  }

  template <typename IntegerType>
  static inline IntegerType parse_integer(
      std::string_view text,
      std::string_view field_name) {
    return massive_speedup::backend_generic::detail::parse_integer<IntegerType>(
        text,
        field_name);
  }

  static inline double parse_double(std::string_view text, std::string_view field_name) {
    return massive_speedup::backend_generic::detail::parse_double(text, field_name);
  }

  template <std::size_t BitCount>
  static inline std::bitset<BitCount> parse_bitset(
      std::string_view text,
      std::string_view field_name) {
    return massive_speedup::backend_generic::detail::parse_bitset<BitCount>(
        text,
        field_name);
  }

  static inline void split_on_commas(
      std::string_view payload,
      std::vector<std::string>& output) {
    if (output.empty()) {
      output.resize(4);
    }

    std::size_t field_index = 0;
    std::size_t start = 0;

    while (true) {
      if (field_index >= output.size()) {
        output.resize(output.size() * 2);
      }

      const auto comma = payload.find(',', start);
      if (comma == std::string_view::npos) {
        output[field_index].assign(payload.substr(start));
        break;
      }

      output[field_index].assign(payload.substr(start, comma - start));
      ++field_index;
      start = comma + 1;
    }

    output.resize(field_index + 1);
  }

  static inline void split_csv_fields(
      std::string_view line,
      std::vector<std::string>& output) {
    if (output.empty()) {
      output.resize(8);
    } else {
      output[0].clear();
    }

    const char* data = line.data();
    const std::size_t size = line.size();
    constexpr std::size_t chunk_size = 32;
    const __m256i comma = _mm256_set1_epi8(',');
    const __m256i quote = _mm256_set1_epi8('"');

    auto ensure_field = [&](std::size_t field_index) {
      if (field_index >= output.size()) {
        output.resize(output.size() * 2);
      }
    };

    auto append_span = [&](std::size_t field_index, std::size_t start, std::size_t end) {
      if (end > start) {
        output[field_index].append(data + start, end - start);
      }
    };

    std::size_t field_index = 0;
    std::size_t segment_start = 0;
    std::size_t index = 0;
    bool in_quotes = false;

    while (size - index >= chunk_size) {
      const __m256i block =
          _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + index));
      int mask = _mm256_movemask_epi8(
          _mm256_or_si256(_mm256_cmpeq_epi8(block, comma), _mm256_cmpeq_epi8(block, quote)));

      if (mask == 0) {
        index += chunk_size;
        continue;
      }

      const auto offset = static_cast<std::size_t>(__builtin_ctz(mask));
      const std::size_t special_index = index + offset;
      const char value = data[special_index];

      if (value == '"') {
        append_span(field_index, segment_start, special_index);
        if (in_quotes && special_index + 1 < size && data[special_index + 1] == '"') {
          output[field_index].push_back('"');
          index = special_index + 2;
          segment_start = index;
        } else {
          in_quotes = !in_quotes;
          index = special_index + 1;
          segment_start = index;
        }
        continue;
      }

      if (value == ',' && !in_quotes) {
        append_span(field_index, segment_start, special_index);
        ++field_index;
        ensure_field(field_index);
        output[field_index].clear();
        index = special_index + 1;
        segment_start = index;
        continue;
      }

      index = special_index + 1;
    }

    for (; index < size; ++index) {
      const char value = data[index];

      if (value == '"') {
        append_span(field_index, segment_start, index);
        if (in_quotes && index + 1 < size && data[index + 1] == '"') {
          output[field_index].push_back('"');
          ++index;
          segment_start = index + 1;
        } else {
          in_quotes = !in_quotes;
          segment_start = index + 1;
        }
        continue;
      }

      if (value == ',' && !in_quotes) {
        append_span(field_index, segment_start, index);
        ++field_index;
        ensure_field(field_index);
        output[field_index].clear();
        segment_start = index + 1;
      }
    }

    append_span(field_index, segment_start, size);
    output.resize(field_index + 1);
  }
};

template <typename Base>
using Implementation = massive_speedup::backend_generic::Implementation<Base, Specialization>;

}  // namespace massive_speedup::backend_x86_avx

NB_MODULE(_avx, m) {
  massive_speedup::bindings::bind_backend_module<massive_speedup::backend_x86_avx::Implementation>(
      m,
      massive_speedup::BackendKind::x86_avx2,
      "_Avx");
}
