#include "massive_speedup/parsers.hpp"

#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <Python.h>

#include <cstddef>
#include <vector>

namespace massive_speedup {

std::string payload_to_string(nb::handle payload) {
  if (nb::isinstance<nb::bytes>(payload)) {
    char* buffer = nullptr;
    Py_ssize_t length = 0;
    if (PyBytes_AsStringAndSize(payload.ptr(), &buffer, &length) != 0) {
      throw nb::python_error();
    }
    return {buffer, static_cast<std::size_t>(length)};
  }

  return nb::cast<std::string>(payload);
}

std::size_t count_byte(std::string_view payload, char byte) {
  std::size_t count = 0;
  for (const char value : payload) {
    count += value == byte;
  }
  return count;
}

std::size_t count_substring(std::string_view payload, std::string_view needle) {
  std::size_t count = 0;
  std::size_t pos = 0;

  while ((pos = payload.find(needle, pos)) != std::string_view::npos) {
    ++count;
    pos += needle.size();
  }

  return count;
}

std::string Parser::serialize() const {
  return "parser_group=" + parser_group() + ";asset_class=" + asset_class() +
         ";processor=" + processor_name();
}

ProcessorType Parser::processor_type() const { return module_processor_type(); }

std::string Parser::processor_name() const {
  return processor_type_name(processor_type());
}

Summary Parser::build_summary(
    std::string_view payload,
    std::string_view operation,
    std::string_view format,
    SplitOnCommasFn split_on_commas) const {
  std::vector<std::string> segments;
  split_on_commas(payload, segments);

  Summary summary;
  summary.emplace("parser_group", nb::cast(parser_group()));
  summary.emplace("asset_class", nb::cast(asset_class()));
  summary.emplace("processor", nb::cast(processor_name()));
  summary.emplace("operation", nb::cast(std::string(operation)));
  summary.emplace("format", nb::cast(std::string(format)));
  summary.emplace("bytes", nb::int_(payload.size()));
  summary.emplace("commas", nb::int_(count_byte(payload, ',')));
  summary.emplace("newlines", nb::int_(count_byte(payload, '\n')));
  summary.emplace("json_objects", nb::int_(count_byte(payload, '{')));
  summary.emplace("segments", nb::int_(segments.size()));
  summary.emplace("event_markers", nb::int_(count_substring(payload, "\"ev\"")));
  return summary;
}

}  // namespace massive_speedup
