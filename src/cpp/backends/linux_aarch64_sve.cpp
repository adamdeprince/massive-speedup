#include "backends/generic.hpp"
#include "module_bindings.hpp"

namespace nb = nanobind;

namespace massive_speedup::backend_linux_aarch64_sve {

struct Specialization {
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

    std::size_t field_index = 0;
    bool in_quotes = false;

    for (std::size_t index = 0; index < line.size(); ++index) {
      const char value = line[index];

      if (value == '"') {
        if (in_quotes && index + 1 < line.size() && line[index + 1] == '"') {
          output[field_index].push_back('"');
          ++index;
        } else {
          in_quotes = !in_quotes;
        }
        continue;
      }

      if (value == ',' && !in_quotes) {
        ++field_index;
        if (field_index >= output.size()) {
          output.resize(output.size() * 2);
        }
        output[field_index].clear();
        continue;
      }

      output[field_index].push_back(value);
    }

    output.resize(field_index + 1);
  }
};

template <typename Base>
using Implementation = massive_speedup::backend_generic::Implementation<Base, Specialization>;

}  // namespace massive_speedup::backend_linux_aarch64_sve

NB_MODULE(_sve, m) {
  massive_speedup::bindings::bind_backend_module<
      massive_speedup::backend_linux_aarch64_sve::Implementation>(
      m,
      massive_speedup::BackendKind::linux_aarch64_sve,
      "_Sve");
}
