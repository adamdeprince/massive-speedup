#include "backends/generic.hpp"
#include "module_bindings.hpp"

namespace nb = nanobind;

namespace massive_speedup::backend_x86_avx {

struct Specialization : massive_speedup::backend_generic::GenericSpecialization {
  static inline void split_on_commas(
      std::string_view payload,
      std::vector<std::string>& output) {
    if (output.empty()) {
      output.resize(1);
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
      output.resize(1);
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

}  // namespace massive_speedup::backend_x86_avx

NB_MODULE(_avx, m) {
  massive_speedup::bindings::bind_backend_module<massive_speedup::backend_x86_avx::Implementation>(
      m,
      massive_speedup::BackendKind::x86_avx2,
      "_Avx");
}
