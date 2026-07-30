#pragma once

#include <nanobind/nanobind.h>

#include <string>
#include <unordered_map>

namespace massive_speedup {

namespace nb = nanobind;

using Summary = std::unordered_map<std::string, nb::object>;

class Parser {
 public:
  virtual ~Parser() = default;

  virtual std::string parser_group() const = 0;
  virtual std::string asset_class() const = 0;

  std::string serialize() const;
  std::string processor_name() const;
};

class FlatFileParser : public Parser {
 public:
  std::string parser_group() const override;

  Summary parse_quotes(nb::handle payload) const;
  Summary parse_trades(nb::handle payload) const;
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

class FlatFileCurrenciesParser : public FlatFileParser {
 public:
  std::string asset_class() const override;
};

class FlatFileCryptoParser : public FlatFileParser {
 public:
  std::string asset_class() const override;
};

}  // namespace massive_speedup
