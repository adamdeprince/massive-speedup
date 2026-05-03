#include "massive_speedup/parsers.hpp"

#include <stdexcept>

namespace massive_speedup {

std::string FlatFileParser::parser_group() const { return "flatfiles"; }

Summary FlatFileParser::parse_quotes(nb::handle payload) const {
  static_cast<void>(payload);
  throw std::runtime_error("flatfile quote parsing must be implemented by a concrete parser");
}

Summary FlatFileParser::parse_minute_aggregates(nb::handle payload) const {
  static_cast<void>(payload);
  throw std::runtime_error(
      "flatfile minute aggregate parsing must be implemented by a concrete parser");
}

Summary FlatFileParser::parse_daily_aggregates(nb::handle payload) const {
  static_cast<void>(payload);
  throw std::runtime_error(
      "flatfile daily aggregate parsing must be implemented by a concrete parser");
}

Summary FlatFileParser::parse_trades(nb::handle payload) const {
  static_cast<void>(payload);
  throw std::runtime_error("flatfile trade parsing must be implemented by a concrete parser");
}

std::string FlatFileStocksParser::asset_class() const { return "stocks"; }
std::string FlatFileOptionsParser::asset_class() const { return "options"; }
std::string FlatFileFuturesParser::asset_class() const { return "futures"; }
std::string FlatFileIndicesParser::asset_class() const { return "indices"; }
std::string FlatFileForexParser::asset_class() const { return "forex"; }
std::string FlatFileCurrenciesParser::asset_class() const { return "currencies"; }
std::string FlatFileCryptoParser::asset_class() const { return "crypto"; }

}  // namespace massive_speedup
