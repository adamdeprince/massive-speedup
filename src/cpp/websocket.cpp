#include "massive_speedup/parsers.hpp"

namespace massive_speedup {

std::string WebSocketParser::parser_group() const { return "websocket"; }

Summary WebSocketParser::parse_message(nb::handle payload) const {
  auto split_on_commas = [](std::string_view payload, std::vector<std::string>& output) {
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
  };

  const std::string materialized = payload_to_string(payload);
  Summary summary = build_summary(materialized, "parse_message", "json", split_on_commas);
  summary.emplace(
      "message_frames",
      nb::int_(
          count_substring(materialized, "},{") + count_substring(materialized, "}{") +
          (materialized.empty() ? 0 : 1)));
  return summary;
}

std::string WebSocketMessagesParser::asset_class() const { return "messages"; }
std::string WebSocketStocksParser::asset_class() const { return "stocks"; }
std::string WebSocketOptionsParser::asset_class() const { return "options"; }
std::string WebSocketFuturesParser::asset_class() const { return "futures"; }
std::string WebSocketIndicesParser::asset_class() const { return "indices"; }
std::string WebSocketForexParser::asset_class() const { return "forex"; }
std::string WebSocketCryptoParser::asset_class() const { return "crypto"; }

}  // namespace massive_speedup
