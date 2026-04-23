#pragma once

#include <nanobind/nanobind.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "massive_speedup/cpu.hpp"

namespace massive_speedup {

namespace nb = nanobind;

using Summary = std::unordered_map<std::string, nb::object>;

std::string payload_to_string(nb::handle payload);
std::size_t count_byte(std::string_view payload, char byte);
std::size_t count_substring(std::string_view payload, std::string_view needle);

class Parser {
 public:
  using SplitOnCommasFn = void (*)(std::string_view payload, std::vector<std::string>& output);

  virtual ~Parser() = default;

  virtual std::string parser_group() const = 0;
  virtual std::string asset_class() const = 0;

  std::string serialize() const;
  ProcessorType processor_type() const;
  std::string processor_name() const;

 protected:
  Summary build_summary(
      std::string_view payload,
      std::string_view operation,
      std::string_view format,
      SplitOnCommasFn split_on_commas) const;
};

class FlatFileParser : public Parser {
 public:
  std::string parser_group() const override;

  Summary parse_quotes(nb::handle payload) const;
  Summary parse_minute_aggregates(nb::handle payload) const;
  Summary parse_daily_aggregates(nb::handle payload) const;
  Summary parse_trades(nb::handle payload) const;
};

class WebSocketParser : public Parser {
 public:
  std::string parser_group() const override;

  Summary parse_message(nb::handle payload) const;
};

class FlatFileStocksParser : public FlatFileParser {
 public:
  std::string asset_class() const override;
};

class FlatFileOptionsParser : public FlatFileParser {
 public:
  std::string asset_class() const override;
};

class FlatFileFuturesParser : public FlatFileParser {
 public:
  std::string asset_class() const override;
};

class FlatFileIndicesParser : public FlatFileParser {
 public:
  std::string asset_class() const override;
};

class FlatFileForexParser : public FlatFileParser {
 public:
  std::string asset_class() const override;
};

class FlatFileCryptoParser : public FlatFileParser {
 public:
  std::string asset_class() const override;
};

class WebSocketMessagesParser : public WebSocketParser {
 public:
  std::string asset_class() const override;
};

class WebSocketStocksParser : public WebSocketParser {
 public:
  std::string asset_class() const override;
};

class WebSocketOptionsParser : public WebSocketParser {
 public:
  std::string asset_class() const override;
};

class WebSocketFuturesParser : public WebSocketParser {
 public:
  std::string asset_class() const override;
};

class WebSocketIndicesParser : public WebSocketParser {
 public:
  std::string asset_class() const override;
};

class WebSocketForexParser : public WebSocketParser {
 public:
  std::string asset_class() const override;
};

class WebSocketCryptoParser : public WebSocketParser {
 public:
  std::string asset_class() const override;
};

}  // namespace massive_speedup
