#pragma once

#include <nanobind/nanobind.h>

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "massive_speedup/simd.hpp"

namespace massive_speedup {

namespace nb = nanobind;

using Summary = std::unordered_map<std::string, nb::object>;

std::string payload_to_string(nb::handle payload);

class Parser {
 public:
  explicit Parser(SimdBackendPtr backend);
  virtual ~Parser() = default;

  Summary parse_summary(nb::handle payload) const;
  virtual std::string serialize() const;

  virtual std::string parser_name() const = 0;
  virtual std::string file_type() const = 0;

  Summary backend_descriptor() const;
  const SimdBackend& backend() const;

 protected:
  virtual void specialize_summary(std::string_view payload, Summary& summary) const = 0;

 private:
  SimdBackendPtr backend_;
};

class PolygonParser final : public Parser {
 public:
  using Parser::Parser;

  std::string parser_name() const override;
  std::string file_type() const override;

 protected:
  void specialize_summary(std::string_view payload, Summary& summary) const override;
};

class S3Parser final : public Parser {
 public:
  using Parser::Parser;

  std::string parser_name() const override;
  std::string file_type() const override;

 protected:
  void specialize_summary(std::string_view payload, Summary& summary) const override;
};

class WebsocketParser final : public Parser {
 public:
  using Parser::Parser;

  std::string parser_name() const override;
  std::string file_type() const override;

 protected:
  void specialize_summary(std::string_view payload, Summary& summary) const override;
};

}  // namespace massive_speedup
