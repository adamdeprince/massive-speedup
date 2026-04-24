#pragma once

#include <nanobind/nanobind.h>
#include <nanobind/make_iterator.h>
#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/unordered_map.h>
#include <nanobind/stl/vector.h>

#include <filesystem>
#include <string>
#include <vector>

#include "backends/generic.hpp"
#include "massive_speedup/parsers.hpp"

namespace massive_speedup::bindings {

namespace nb = nanobind;

inline nb::object find_existing_backend_attr(const char* attr_name) {
  static constexpr const char* backend_modules[] = {
      "massive_speedup._generic",
      "massive_speedup._sse",
      "massive_speedup._avx",
      "massive_speedup._avx512",
      "massive_speedup._neon",
      "massive_speedup._sve",
      "massive_speedup._sve2",
      "massive_speedup._lsx",
      "massive_speedup._lasx",
  };

  PyObject* modules = PyImport_GetModuleDict();
  for (const char* module_name : backend_modules) {
    PyObject* module = PyDict_GetItemString(modules, module_name);
    if (module == nullptr || !PyObject_HasAttrString(module, attr_name)) {
      continue;
    }

    PyObject* value = PyObject_GetAttrString(module, attr_name);
    if (value == nullptr) {
      throw nb::python_error();
    }
    return nb::steal<nb::object>(value);
  }

  return nb::object();
}

template <typename ParserType>
std::vector<nb::bytes> read_gzip_lines_bytes(
    const std::filesystem::path& path,
    std::size_t parallelization,
    std::size_t chunk_size) {
  std::vector<nb::bytes> lines;
  for (auto&& line : ParserType::read_gzip_lines(path, parallelization, chunk_size)) {
    lines.emplace_back(line.data(), line.size());
  }
  return lines;
}

template <typename ParserType>
Summary parse_quotes_static(nb::handle payload) {
  return ParserType{}.parse_quotes(payload);
}

template <typename ParserType>
Summary parse_minute_aggregates_static(nb::handle payload) {
  return ParserType{}.parse_minute_aggregates(payload);
}

template <typename ParserType>
Summary parse_daily_aggregates_static(nb::handle payload) {
  return ParserType{}.parse_daily_aggregates(payload);
}

template <typename ParserType>
Summary parse_trades_static(nb::handle payload) {
  return ParserType{}.parse_trades(payload);
}

template <typename ParserType>
Summary parse_message_static(nb::handle payload) {
  return ParserType{}.parse_message(payload);
}

template <typename ParserType>
std::string serialize_static() {
  return ParserType{}.serialize();
}

template <typename ParserType>
std::string processor_name_static() {
  return ParserType{}.processor_name();
}

template <typename ParserType>
ProcessorType processor_type_static() {
  return ParserType{}.processor_type();
}

template <typename ParserType>
BackendKind backend_kind_static() {
  return module_backend_kind();
}

template <typename ParserType>
using GzipLinesIterator = typename ParserType::GzipLinesIterator;

template <typename ParserType>
using StockTradeRowsIterator = typename ParserType::StockTradeRowsIterator;

template <typename ParserType>
using StockQuoteRowsIterator = typename ParserType::StockQuoteRowsIterator;

template <typename ParserType>
using StockAggregateRowsIterator = typename ParserType::StockAggregateRowsIterator;

template <typename ParserType>
using RawStockTradeRowsIterator = typename ParserType::RawStockTradeRowsIterator;

template <typename ParserType>
using RawStockQuoteRowsIterator = typename ParserType::RawStockQuoteRowsIterator;

template <typename ParserType>
using RawStockAggregateRowsIterator = typename ParserType::RawStockAggregateRowsIterator;

template <typename ParserType>
using CurrencyQuoteRowsIterator = typename ParserType::CurrencyQuoteRowsIterator;

template <typename ParserType>
using RawCurrencyQuoteRowsIterator = typename ParserType::RawCurrencyQuoteRowsIterator;

template <typename ParserType>
using CurrencyAggregateRowsIterator = typename ParserType::CurrencyAggregateRowsIterator;

template <typename ParserType>
using RawCurrencyAggregateRowsIterator = typename ParserType::RawCurrencyAggregateRowsIterator;

template <typename ParserType>
using RawLineRowsIterator = typename ParserType::RawLineRowsIterator;

template <typename ParserType>
struct StockTradeApi {};

template <typename ParserType>
struct StockQuoteApi {};

template <typename ParserType>
struct StockAggregateApi {};

template <typename ParserType>
struct CurrencyQuoteApi {};

template <typename ParserType>
struct CurrencyAggregateApi {};

template <typename IteratorType>
void bind_iterator_type(nb::module_& m, const char* iterator_name) {
  nb::class_<IteratorType>(m, iterator_name)
      .def(
          "__iter__",
          [](IteratorType& self) -> IteratorType& { return self.iter(); },
          nb::rv_policy::reference_internal)
      .def("__next__", &IteratorType::next);
}

template <typename ParserType>
void bind_gzip_lines(nb::module_& m, const char* iterator_name) {
  using IteratorType = GzipLinesIterator<ParserType>;

  bind_iterator_type<IteratorType>(m, iterator_name);

  m.def(
      "gzip_lines",
      [](const std::filesystem::path& path,
         std::size_t parallelization,
         std::size_t chunk_size) {
        return IteratorType(path, parallelization, chunk_size);
      },
      nb::arg("path"),
      nb::arg("parallelization") = 0,
      nb::arg("chunk_size") = 1U << 20);
}

template <typename ParserType>
void bind_trade_rows_iterator(nb::module_& m, const char* iterator_name) {
  bind_iterator_type<StockTradeRowsIterator<ParserType>>(m, iterator_name);
}

template <typename ParserType>
void bind_quote_rows_iterator(nb::module_& m, const char* iterator_name) {
  bind_iterator_type<StockQuoteRowsIterator<ParserType>>(m, iterator_name);
}

template <typename ParserType>
void bind_stock_aggregate_rows_iterator(nb::module_& m, const char* iterator_name) {
  bind_iterator_type<StockAggregateRowsIterator<ParserType>>(m, iterator_name);
}

template <typename ParserType>
void bind_raw_trade_rows_iterator(nb::module_& m, const char* iterator_name) {
  bind_iterator_type<RawStockTradeRowsIterator<ParserType>>(m, iterator_name);
}

template <typename ParserType>
void bind_raw_quote_rows_iterator(nb::module_& m, const char* iterator_name) {
  bind_iterator_type<RawStockQuoteRowsIterator<ParserType>>(m, iterator_name);
}

template <typename ParserType>
void bind_raw_stock_aggregate_rows_iterator(nb::module_& m, const char* iterator_name) {
  bind_iterator_type<RawStockAggregateRowsIterator<ParserType>>(m, iterator_name);
}

template <typename ParserType>
void bind_currency_quote_rows_iterator(nb::module_& m, const char* iterator_name) {
  bind_iterator_type<CurrencyQuoteRowsIterator<ParserType>>(m, iterator_name);
}

template <typename ParserType>
void bind_raw_currency_quote_rows_iterator(nb::module_& m, const char* iterator_name) {
  bind_iterator_type<RawCurrencyQuoteRowsIterator<ParserType>>(m, iterator_name);
}

template <typename ParserType>
void bind_currency_aggregate_rows_iterator(nb::module_& m, const char* iterator_name) {
  bind_iterator_type<CurrencyAggregateRowsIterator<ParserType>>(m, iterator_name);
}

template <typename ParserType>
void bind_raw_currency_aggregate_rows_iterator(nb::module_& m, const char* iterator_name) {
  bind_iterator_type<RawCurrencyAggregateRowsIterator<ParserType>>(m, iterator_name);
}

template <typename ParserType>
void bind_raw_line_rows_iterator(nb::module_& m, const char* iterator_name) {
  bind_iterator_type<RawLineRowsIterator<ParserType>>(m, iterator_name);
}

template <typename RowType>
void bind_participant_timestamp_ordering(nb::class_<RowType>& class_) {
  class_
      .def(
          "__lt__",
          [](const RowType& lhs, const RowType& rhs) {
            return lhs.participant_timestamp < rhs.participant_timestamp;
          },
          nb::is_operator())
      .def(
          "__le__",
          [](const RowType& lhs, const RowType& rhs) {
            return lhs.participant_timestamp <= rhs.participant_timestamp;
          },
          nb::is_operator())
      .def(
          "__gt__",
          [](const RowType& lhs, const RowType& rhs) {
            return lhs.participant_timestamp > rhs.participant_timestamp;
          },
          nb::is_operator())
      .def(
          "__ge__",
          [](const RowType& lhs, const RowType& rhs) {
            return lhs.participant_timestamp >= rhs.participant_timestamp;
          },
          nb::is_operator());
}

template <typename RowType>
void bind_window_start_ordering(nb::class_<RowType>& class_) {
  class_
      .def(
          "__lt__",
          [](const RowType& lhs, const RowType& rhs) {
            return lhs.window_start < rhs.window_start;
          },
          nb::is_operator())
      .def(
          "__le__",
          [](const RowType& lhs, const RowType& rhs) {
            return lhs.window_start <= rhs.window_start;
          },
          nb::is_operator())
      .def(
          "__gt__",
          [](const RowType& lhs, const RowType& rhs) {
            return lhs.window_start > rhs.window_start;
          },
          nb::is_operator())
      .def(
          "__ge__",
          [](const RowType& lhs, const RowType& rhs) {
            return lhs.window_start >= rhs.window_start;
          },
          nb::is_operator());
}

template <typename Specialization>
inline void bind_row_models(nb::module_& m, nb::module_& flatfiles) {
  if (nb::object existing = find_existing_backend_attr("StockTrade"); existing.is_valid()) {
    m.attr("StockTrade") = existing;
    m.attr("StockQuote") = find_existing_backend_attr("StockQuote");
    m.attr("StockAggregate") = find_existing_backend_attr("StockAggregate");
    m.attr("CurrencyQuote") = find_existing_backend_attr("CurrencyQuote");
    m.attr("CurrencyAggregate") = find_existing_backend_attr("CurrencyAggregate");
    m.attr("StockQuotes") = m.attr("StockQuote");
    flatfiles.attr("StockTrade") = m.attr("StockTrade");
    flatfiles.attr("StockQuote") = m.attr("StockQuote");
    flatfiles.attr("StockAggregate") = m.attr("StockAggregate");
    flatfiles.attr("CurrencyQuote") = m.attr("CurrencyQuote");
    flatfiles.attr("CurrencyAggregate") = m.attr("CurrencyAggregate");
    flatfiles.attr("StockQuotes") = m.attr("StockQuote");
    return;
  }

  auto stock_trade =
      nb::class_<backend_generic::StockTrade>(m, "StockTrade")
          .def(
              "__init__",
              [](backend_generic::StockTrade* self,
                 const std::vector<std::string>& fields) {
                new (self) backend_generic::StockTrade(
                    backend_generic::StockTrade::template from_fields<Specialization>(fields));
              },
              nb::arg("fields"))
          .def_ro("ticker", &backend_generic::StockTrade::ticker)
          .def_prop_ro("conditions", &backend_generic::StockTrade::conditions_object)
          .def_ro("correction", &backend_generic::StockTrade::correction)
          .def_ro("exchange", &backend_generic::StockTrade::exchange)
          .def_ro("id", &backend_generic::StockTrade::id)
          .def_ro(
              "participant_timestamp",
              &backend_generic::StockTrade::participant_timestamp)
          .def_ro("price", &backend_generic::StockTrade::price)
          .def_ro(
              "sequence_number",
              &backend_generic::StockTrade::sequence_number)
          .def_ro("sip_timestamp", &backend_generic::StockTrade::sip_timestamp)
          .def_ro("size", &backend_generic::StockTrade::size)
          .def_ro("tape", &backend_generic::StockTrade::tape)
          .def_ro("trf_id", &backend_generic::StockTrade::trf_id)
          .def_ro("trf_timestamp", &backend_generic::StockTrade::trf_timestamp)
          .def(
              "__iter__",
              [](const backend_generic::StockTrade& self) {
                return backend_generic::detail::tuple_iterator(self.python_fields());
              })
          .def("__hash__", &backend_generic::StockTrade::hash_value)
          .def("__str__", &backend_generic::StockTrade::repr)
          .def("__repr__", &backend_generic::StockTrade::repr)
          .def(
              "__eq__",
              [](const backend_generic::StockTrade& lhs,
                 const backend_generic::StockTrade& rhs) { return lhs == rhs; },
              nb::is_operator());
  bind_participant_timestamp_ordering(stock_trade);

  auto stock_quote =
      nb::class_<backend_generic::StockQuote>(m, "StockQuote")
          .def(
              "__init__",
              [](backend_generic::StockQuote* self,
                 const std::vector<std::string>& fields) {
                new (self) backend_generic::StockQuote(
                    backend_generic::StockQuote::template from_fields<Specialization>(fields));
              },
              nb::arg("fields"))
          .def_ro("ticker", &backend_generic::StockQuote::ticker)
          .def_ro("ask_exchange", &backend_generic::StockQuote::ask_exchange)
          .def_ro("ask_price", &backend_generic::StockQuote::ask_price)
          .def_ro("ask_size", &backend_generic::StockQuote::ask_size)
          .def_ro("bid_exchange", &backend_generic::StockQuote::bid_exchange)
          .def_ro("bid_price", &backend_generic::StockQuote::bid_price)
          .def_ro("bid_size", &backend_generic::StockQuote::bid_size)
          .def_prop_ro("conditions", &backend_generic::StockQuote::conditions_object)
          .def_prop_ro("indicators", &backend_generic::StockQuote::indicators_object)
          .def_ro(
              "participant_timestamp",
              &backend_generic::StockQuote::participant_timestamp)
          .def_ro(
              "sequence_number",
              &backend_generic::StockQuote::sequence_number)
          .def_ro("sip_timestamp", &backend_generic::StockQuote::sip_timestamp)
          .def_ro("tape", &backend_generic::StockQuote::tape)
          .def_ro("trf_timestamp", &backend_generic::StockQuote::trf_timestamp)
          .def(
              "__iter__",
              [](const backend_generic::StockQuote& self) {
                return backend_generic::detail::tuple_iterator(self.python_fields());
              })
          .def("__hash__", &backend_generic::StockQuote::hash_value)
          .def("__str__", &backend_generic::StockQuote::repr)
          .def("__repr__", &backend_generic::StockQuote::repr)
          .def(
              "__eq__",
              [](const backend_generic::StockQuote& lhs,
                 const backend_generic::StockQuote& rhs) { return lhs == rhs; },
              nb::is_operator());
  bind_participant_timestamp_ordering(stock_quote);

  auto currency_quote =
      nb::class_<backend_generic::CurrencyQuote>(m, "CurrencyQuote")
          .def(
              "__init__",
              [](backend_generic::CurrencyQuote* self,
                 const std::vector<std::string>& fields) {
                new (self) backend_generic::CurrencyQuote(
                    backend_generic::CurrencyQuote::template from_fields<Specialization>(fields));
              },
              nb::arg("fields"))
          .def_ro("ticker", &backend_generic::CurrencyQuote::ticker)
          .def_ro("ask_exchange", &backend_generic::CurrencyQuote::ask_exchange)
          .def_ro("ask_price", &backend_generic::CurrencyQuote::ask_price)
          .def_ro("bid_exchange", &backend_generic::CurrencyQuote::bid_exchange)
          .def_ro("bid_price", &backend_generic::CurrencyQuote::bid_price)
          .def_prop_ro("tickers", &backend_generic::CurrencyQuote::tickers_object)
          .def_ro(
              "participant_timestamp",
              &backend_generic::CurrencyQuote::participant_timestamp)
          .def(
              "__iter__",
              [](const backend_generic::CurrencyQuote& self) {
                return backend_generic::detail::tuple_iterator(self.python_fields());
              })
          .def("__hash__", &backend_generic::CurrencyQuote::hash_value)
          .def("__str__", &backend_generic::CurrencyQuote::repr)
          .def("__repr__", &backend_generic::CurrencyQuote::repr)
          .def(
              "__eq__",
              [](const backend_generic::CurrencyQuote& lhs,
                 const backend_generic::CurrencyQuote& rhs) { return lhs == rhs; },
              nb::is_operator());
  bind_participant_timestamp_ordering(currency_quote);

  auto stock_aggregate =
      nb::class_<backend_generic::StockAggregate>(m, "StockAggregate")
          .def(
              "__init__",
              [](backend_generic::StockAggregate* self,
                 const std::vector<std::string>& fields) {
                new (self) backend_generic::StockAggregate(
                    backend_generic::StockAggregate::template from_fields<Specialization>(fields));
              },
              nb::arg("fields"))
          .def_ro("ticker", &backend_generic::StockAggregate::ticker)
          .def_ro("volume", &backend_generic::StockAggregate::volume)
          .def_ro("open", &backend_generic::StockAggregate::open)
          .def_ro("close", &backend_generic::StockAggregate::close)
          .def_ro("high", &backend_generic::StockAggregate::high)
          .def_ro("low", &backend_generic::StockAggregate::low)
          .def_ro("window_start", &backend_generic::StockAggregate::window_start)
          .def_ro("transactions", &backend_generic::StockAggregate::transactions)
          .def(
              "__iter__",
              [](const backend_generic::StockAggregate& self) {
                return backend_generic::detail::tuple_iterator(self.python_fields());
              })
          .def("__hash__", &backend_generic::StockAggregate::hash_value)
          .def("__str__", &backend_generic::StockAggregate::repr)
          .def("__repr__", &backend_generic::StockAggregate::repr)
          .def(
              "__eq__",
              [](const backend_generic::StockAggregate& lhs,
                 const backend_generic::StockAggregate& rhs) { return lhs == rhs; },
              nb::is_operator());
  bind_window_start_ordering(stock_aggregate);

  auto currency_aggregate =
      nb::class_<backend_generic::CurrencyAggregate>(m, "CurrencyAggregate")
          .def(
              "__init__",
              [](backend_generic::CurrencyAggregate* self,
                 const std::vector<std::string>& fields) {
                new (self) backend_generic::CurrencyAggregate(
                    backend_generic::CurrencyAggregate::template from_fields<Specialization>(fields));
              },
              nb::arg("fields"))
          .def_ro("ticker", &backend_generic::CurrencyAggregate::ticker)
          .def_ro("volume", &backend_generic::CurrencyAggregate::volume)
          .def_ro("open", &backend_generic::CurrencyAggregate::open)
          .def_ro("close", &backend_generic::CurrencyAggregate::close)
          .def_ro("high", &backend_generic::CurrencyAggregate::high)
          .def_ro("low", &backend_generic::CurrencyAggregate::low)
          .def_ro("window_start", &backend_generic::CurrencyAggregate::window_start)
          .def_ro("transactions", &backend_generic::CurrencyAggregate::transactions)
          .def_prop_ro("tickers", &backend_generic::CurrencyAggregate::tickers_object)
          .def(
              "__iter__",
              [](const backend_generic::CurrencyAggregate& self) {
                return backend_generic::detail::tuple_iterator(self.python_fields());
              })
          .def("__hash__", &backend_generic::CurrencyAggregate::hash_value)
          .def("__str__", &backend_generic::CurrencyAggregate::repr)
          .def("__repr__", &backend_generic::CurrencyAggregate::repr)
          .def(
              "__eq__",
              [](const backend_generic::CurrencyAggregate& lhs,
                 const backend_generic::CurrencyAggregate& rhs) { return lhs == rhs; },
              nb::is_operator());
  bind_window_start_ordering(currency_aggregate);

  flatfiles.attr("StockTrade") = m.attr("StockTrade");
  flatfiles.attr("StockQuote") = m.attr("StockQuote");
  flatfiles.attr("StockAggregate") = m.attr("StockAggregate");
  flatfiles.attr("CurrencyQuote") = m.attr("CurrencyQuote");
  flatfiles.attr("CurrencyAggregate") = m.attr("CurrencyAggregate");
  flatfiles.attr("StockQuotes") = m.attr("StockQuote");
  m.attr("StockQuotes") = m.attr("StockQuote");
}

inline void bind_common_bases(nb::module_& m) {
  if (nb::object existing = find_existing_backend_attr("Parser"); existing.is_valid()) {
    m.attr("Parser") = existing;
    m.attr("FlatFileParser") = find_existing_backend_attr("FlatFileParser");
    m.attr("WebSocketParser") = find_existing_backend_attr("WebSocketParser");
    m.attr("FlatFileStocksParser") = find_existing_backend_attr("FlatFileStocksParser");
    m.attr("FlatFileOptionsParser") = find_existing_backend_attr("FlatFileOptionsParser");
    m.attr("FlatFileFuturesParser") = find_existing_backend_attr("FlatFileFuturesParser");
    m.attr("FlatFileIndicesParser") = find_existing_backend_attr("FlatFileIndicesParser");
    m.attr("FlatFileForexParser") = find_existing_backend_attr("FlatFileForexParser");
    m.attr("FlatFileCurrenciesParser") = find_existing_backend_attr("FlatFileCurrenciesParser");
    m.attr("FlatFileCryptoParser") = find_existing_backend_attr("FlatFileCryptoParser");
    m.attr("WebSocketMessagesParser") = find_existing_backend_attr("WebSocketMessagesParser");
    m.attr("WebSocketStocksParser") = find_existing_backend_attr("WebSocketStocksParser");
    m.attr("WebSocketOptionsParser") = find_existing_backend_attr("WebSocketOptionsParser");
    m.attr("WebSocketFuturesParser") = find_existing_backend_attr("WebSocketFuturesParser");
    m.attr("WebSocketIndicesParser") = find_existing_backend_attr("WebSocketIndicesParser");
    m.attr("WebSocketForexParser") = find_existing_backend_attr("WebSocketForexParser");
    m.attr("WebSocketCryptoParser") = find_existing_backend_attr("WebSocketCryptoParser");
    return;
  }

  nb::class_<Parser>(m, "Parser");

  nb::class_<FlatFileParser, Parser>(m, "FlatFileParser");

  nb::class_<WebSocketParser, Parser>(m, "WebSocketParser");

  nb::class_<FlatFileStocksParser, FlatFileParser>(m, "FlatFileStocksParser");
  nb::class_<FlatFileOptionsParser, FlatFileParser>(m, "FlatFileOptionsParser");
  nb::class_<FlatFileFuturesParser, FlatFileParser>(m, "FlatFileFuturesParser");
  nb::class_<FlatFileIndicesParser, FlatFileParser>(m, "FlatFileIndicesParser");
  nb::class_<FlatFileForexParser, FlatFileParser>(m, "FlatFileForexParser");
  nb::class_<FlatFileCurrenciesParser, FlatFileParser>(m, "FlatFileCurrenciesParser");
  nb::class_<FlatFileCryptoParser, FlatFileParser>(m, "FlatFileCryptoParser");

  nb::class_<WebSocketMessagesParser, WebSocketParser>(m, "WebSocketMessagesParser");
  nb::class_<WebSocketStocksParser, WebSocketParser>(m, "WebSocketStocksParser");
  nb::class_<WebSocketOptionsParser, WebSocketParser>(m, "WebSocketOptionsParser");
  nb::class_<WebSocketFuturesParser, WebSocketParser>(m, "WebSocketFuturesParser");
  nb::class_<WebSocketIndicesParser, WebSocketParser>(m, "WebSocketIndicesParser");
  nb::class_<WebSocketForexParser, WebSocketParser>(m, "WebSocketForexParser");
  nb::class_<WebSocketCryptoParser, WebSocketParser>(m, "WebSocketCryptoParser");
}

template <typename BaseAsset, typename ImplAsset>
void bind_flatfile_asset(nb::module_& module, const char* name) {
  nb::class_<ImplAsset, BaseAsset>(module, name)
      .def(nb::init<>())
      .def_static("parse_quotes", &parse_quotes_static<ImplAsset>, nb::arg("payload"))
      .def_static(
          "parse_minute_aggregates",
          &parse_minute_aggregates_static<ImplAsset>,
          nb::arg("payload"))
      .def_static(
          "parse_daily_aggregates",
          &parse_daily_aggregates_static<ImplAsset>,
          nb::arg("payload"))
      .def_static("parse_trades", &parse_trades_static<ImplAsset>, nb::arg("payload"))
      .def_static("serialize", &serialize_static<ImplAsset>)
      .def_static("processor_name", &processor_name_static<ImplAsset>)
      .def_static("processor_type", &processor_type_static<ImplAsset>)
      .def_static("backend_kind", &backend_kind_static<ImplAsset>);
}

template <typename BaseAsset, typename ImplAsset>
void bind_stock_flatfile_asset(
    nb::module_& module,
    const char* name,
    const char* trade_iterator_name,
    const char* quote_iterator_name,
    const char* aggregate_iterator_name,
    const char* raw_trade_iterator_name,
    const char* raw_quote_iterator_name,
    const char* raw_aggregate_iterator_name,
    const char* raw_line_iterator_name,
    const char* trade_api_name,
    const char* quote_api_name,
    const char* aggregate_api_name) {
  using TradeIterator = StockTradeRowsIterator<ImplAsset>;
  using QuoteIterator = StockQuoteRowsIterator<ImplAsset>;
  using AggregateIterator = StockAggregateRowsIterator<ImplAsset>;
  using RawTradeIterator = RawStockTradeRowsIterator<ImplAsset>;
  using RawQuoteIterator = RawStockQuoteRowsIterator<ImplAsset>;
  using RawAggregateIterator = RawStockAggregateRowsIterator<ImplAsset>;
  using RawLineIterator = RawLineRowsIterator<ImplAsset>;
  using TradeApi = StockTradeApi<ImplAsset>;
  using QuoteApi = StockQuoteApi<ImplAsset>;
  using AggregateApi = StockAggregateApi<ImplAsset>;

  bind_trade_rows_iterator<ImplAsset>(module, trade_iterator_name);
  bind_quote_rows_iterator<ImplAsset>(module, quote_iterator_name);
  bind_stock_aggregate_rows_iterator<ImplAsset>(module, aggregate_iterator_name);
  bind_raw_trade_rows_iterator<ImplAsset>(module, raw_trade_iterator_name);
  bind_raw_quote_rows_iterator<ImplAsset>(module, raw_quote_iterator_name);
  bind_raw_stock_aggregate_rows_iterator<ImplAsset>(module, raw_aggregate_iterator_name);
  bind_raw_line_rows_iterator<ImplAsset>(module, raw_line_iterator_name);

  nb::class_<ImplAsset, BaseAsset> stock_class(module, name);
  stock_class
      .def(nb::init<>())
      .def_static(
          "parse_quotes",
          [](const std::filesystem::path& path,
             bool sort_by_participant_timestamp,
             bool sort_by_sip_timestamp) {
            return QuoteIterator(
                path,
                sort_by_participant_timestamp,
                sort_by_sip_timestamp);
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("sort_by_participant_timestamp") = false,
          nb::arg("sort_by_sip_timestamp") = false)
      .def_static(
          "parse_raw_quotes",
          [](const std::filesystem::path& path,
             bool sort_by_participant_timestamp,
             bool sort_by_sip_timestamp) {
            return RawQuoteIterator(
                path,
                sort_by_participant_timestamp,
                sort_by_sip_timestamp);
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("sort_by_participant_timestamp") = false,
          nb::arg("sort_by_sip_timestamp") = false)
      .def_static(
          "parse_minute_aggregates",
          [](const std::filesystem::path& path, bool sort_by_window_start) {
            return AggregateIterator(path, sort_by_window_start);
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("sort_by_window_start") = false)
      .def_static(
          "parse_daily_aggregates",
          [](const std::filesystem::path& path, bool sort_by_window_start) {
            return AggregateIterator(path, sort_by_window_start);
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("sort_by_window_start") = false)
      .def_static(
          "parse_raw_minute_aggregates",
          [](const std::filesystem::path& path, bool sort_by_window_start) {
            return RawAggregateIterator(path, sort_by_window_start);
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("sort_by_window_start") = false)
      .def_static(
          "parse_raw_daily_aggregates",
          [](const std::filesystem::path& path, bool sort_by_window_start) {
            return RawAggregateIterator(path, sort_by_window_start);
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("sort_by_window_start") = false)
      .def_static(
          "parse_trades",
          [](const std::filesystem::path& path,
             bool sort_by_participant_timestamp,
             bool sort_by_sip_timestamp) {
            return TradeIterator(
                path,
                sort_by_participant_timestamp,
                sort_by_sip_timestamp);
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("sort_by_participant_timestamp") = false,
          nb::arg("sort_by_sip_timestamp") = false)
      .def_static(
          "parse_raw_trades",
          [](const std::filesystem::path& path,
             bool sort_by_participant_timestamp,
             bool sort_by_sip_timestamp) {
            return RawTradeIterator(
                path,
                sort_by_participant_timestamp,
                sort_by_sip_timestamp);
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("sort_by_participant_timestamp") = false,
          nb::arg("sort_by_sip_timestamp") = false)
      .def_static(
          "raw_lines",
          [](const std::filesystem::path& path) {
            return RawLineIterator(path);
          },
          nb::arg("path"))
      .def_static("serialize", &serialize_static<ImplAsset>)
      .def_static("processor_name", &processor_name_static<ImplAsset>)
      .def_static("processor_type", &processor_type_static<ImplAsset>)
      .def_static("backend_kind", &backend_kind_static<ImplAsset>);

  nb::class_<TradeApi>(module, trade_api_name)
      .def_static(
          "parse",
          [](const std::filesystem::path& path,
             bool sort_by_participant_timestamp,
             bool sort_by_sip_timestamp) {
            return TradeIterator(
                path,
                sort_by_participant_timestamp,
                sort_by_sip_timestamp);
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("sort_by_participant_timestamp") = false,
          nb::arg("sort_by_sip_timestamp") = false)
      .def_static(
          "parse_raw",
          [](const std::filesystem::path& path,
             bool sort_by_participant_timestamp,
             bool sort_by_sip_timestamp) {
            return RawTradeIterator(
                path,
                sort_by_participant_timestamp,
                sort_by_sip_timestamp);
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("sort_by_participant_timestamp") = false,
          nb::arg("sort_by_sip_timestamp") = false)
      .def_static(
          "raw_lines",
          [](const std::filesystem::path& path) {
            return RawLineIterator(path);
          },
          nb::arg("path"));

  nb::class_<QuoteApi>(module, quote_api_name)
      .def_static(
          "parse",
          [](const std::filesystem::path& path,
             bool sort_by_participant_timestamp,
             bool sort_by_sip_timestamp) {
            return QuoteIterator(
                path,
                sort_by_participant_timestamp,
                sort_by_sip_timestamp);
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("sort_by_participant_timestamp") = false,
          nb::arg("sort_by_sip_timestamp") = false)
      .def_static(
          "parse_raw",
          [](const std::filesystem::path& path,
             bool sort_by_participant_timestamp,
             bool sort_by_sip_timestamp) {
            return RawQuoteIterator(
                path,
                sort_by_participant_timestamp,
                sort_by_sip_timestamp);
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("sort_by_participant_timestamp") = false,
          nb::arg("sort_by_sip_timestamp") = false)
      .def_static(
          "raw_lines",
          [](const std::filesystem::path& path) {
            return RawLineIterator(path);
          },
          nb::arg("path"));

  nb::class_<AggregateApi>(module, aggregate_api_name)
      .def_static(
          "parse",
          [](const std::filesystem::path& path, bool sort_by_window_start) {
            return AggregateIterator(path, sort_by_window_start);
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("sort_by_window_start") = false)
      .def_static(
          "parse_raw",
          [](const std::filesystem::path& path, bool sort_by_window_start) {
            return RawAggregateIterator(path, sort_by_window_start);
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("sort_by_window_start") = false)
      .def_static(
          "raw_lines",
          [](const std::filesystem::path& path) {
            return RawLineIterator(path);
          },
          nb::arg("path"));

  stock_class.attr("Trade") = module.attr(trade_api_name);
  stock_class.attr("Quote") = module.attr(quote_api_name);
  stock_class.attr("Aggregate") = module.attr(aggregate_api_name);
}

template <typename BaseAsset, typename ImplAsset>
void bind_websocket_asset(nb::module_& module, const char* name) {
  nb::class_<ImplAsset, BaseAsset>(module, name)
      .def(nb::init<>())
      .def_static("parse_message", &parse_message_static<ImplAsset>, nb::arg("payload"))
      .def_static("serialize", &serialize_static<ImplAsset>)
      .def_static("processor_name", &processor_name_static<ImplAsset>)
      .def_static("processor_type", &processor_type_static<ImplAsset>)
      .def_static("backend_kind", &backend_kind_static<ImplAsset>);
}

template <typename BaseAsset, typename ImplAsset>
void bind_currency_flatfile_asset(
    nb::module_& module,
    const char* name,
    const char* quote_iterator_name,
    const char* raw_quote_iterator_name,
    const char* aggregate_iterator_name,
    const char* raw_aggregate_iterator_name,
    const char* raw_line_iterator_name,
    const char* quote_api_name,
    const char* aggregate_api_name) {
  using QuoteIterator = CurrencyQuoteRowsIterator<ImplAsset>;
  using RawQuoteIterator = RawCurrencyQuoteRowsIterator<ImplAsset>;
  using AggregateIterator = CurrencyAggregateRowsIterator<ImplAsset>;
  using RawAggregateIterator = RawCurrencyAggregateRowsIterator<ImplAsset>;
  using RawLineIterator = RawLineRowsIterator<ImplAsset>;
  using QuoteApi = CurrencyQuoteApi<ImplAsset>;
  using AggregateApi = CurrencyAggregateApi<ImplAsset>;

  bind_currency_quote_rows_iterator<ImplAsset>(module, quote_iterator_name);
  bind_raw_currency_quote_rows_iterator<ImplAsset>(module, raw_quote_iterator_name);
  bind_currency_aggregate_rows_iterator<ImplAsset>(module, aggregate_iterator_name);
  bind_raw_currency_aggregate_rows_iterator<ImplAsset>(module, raw_aggregate_iterator_name);

  nb::class_<ImplAsset, BaseAsset> currency_class(module, name);
  currency_class
      .def(nb::init<>())
      .def_static(
          "parse_quotes",
          [](const std::filesystem::path& path,
             bool sort_by_participant_timestamp,
             bool sort_by_sip_timestamp) {
            return QuoteIterator(
                path,
                sort_by_participant_timestamp,
                sort_by_sip_timestamp);
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("sort_by_participant_timestamp") = false,
          nb::arg("sort_by_sip_timestamp") = false)
      .def_static(
          "parse_raw_quotes",
          [](const std::filesystem::path& path,
             bool sort_by_participant_timestamp,
             bool sort_by_sip_timestamp) {
            return RawQuoteIterator(
                path,
                sort_by_participant_timestamp,
                sort_by_sip_timestamp);
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("sort_by_participant_timestamp") = false,
          nb::arg("sort_by_sip_timestamp") = false)
      .def_static(
          "parse_minute_aggregates",
          [](const std::filesystem::path& path, bool sort_by_window_start) {
            return AggregateIterator(path, sort_by_window_start);
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("sort_by_window_start") = false)
      .def_static(
          "parse_daily_aggregates",
          [](const std::filesystem::path& path, bool sort_by_window_start) {
            return AggregateIterator(path, sort_by_window_start);
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("sort_by_window_start") = false)
      .def_static(
          "parse_raw_minute_aggregates",
          [](const std::filesystem::path& path, bool sort_by_window_start) {
            return RawAggregateIterator(path, sort_by_window_start);
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("sort_by_window_start") = false)
      .def_static(
          "parse_raw_daily_aggregates",
          [](const std::filesystem::path& path, bool sort_by_window_start) {
            return RawAggregateIterator(path, sort_by_window_start);
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("sort_by_window_start") = false)
      .def_static(
          "raw_lines",
          [](const std::filesystem::path& path) {
            return RawLineIterator(path);
          },
          nb::arg("path"))
      .def_static("serialize", &serialize_static<ImplAsset>)
      .def_static("processor_name", &processor_name_static<ImplAsset>)
      .def_static("processor_type", &processor_type_static<ImplAsset>)
      .def_static("backend_kind", &backend_kind_static<ImplAsset>);

  nb::class_<QuoteApi>(module, quote_api_name)
      .def_static(
          "parse",
          [](const std::filesystem::path& path,
             bool sort_by_participant_timestamp,
             bool sort_by_sip_timestamp) {
            return QuoteIterator(
                path,
                sort_by_participant_timestamp,
                sort_by_sip_timestamp);
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("sort_by_participant_timestamp") = false,
          nb::arg("sort_by_sip_timestamp") = false)
      .def_static(
          "parse_raw",
          [](const std::filesystem::path& path,
             bool sort_by_participant_timestamp,
             bool sort_by_sip_timestamp) {
            return RawQuoteIterator(
                path,
                sort_by_participant_timestamp,
                sort_by_sip_timestamp);
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("sort_by_participant_timestamp") = false,
          nb::arg("sort_by_sip_timestamp") = false)
      .def_static(
          "raw_lines",
          [](const std::filesystem::path& path) {
            return RawLineIterator(path);
          },
          nb::arg("path"));

  nb::class_<AggregateApi>(module, aggregate_api_name)
      .def_static(
          "parse",
          [](const std::filesystem::path& path, bool sort_by_window_start) {
            return AggregateIterator(path, sort_by_window_start);
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("sort_by_window_start") = false)
      .def_static(
          "parse_raw",
          [](const std::filesystem::path& path, bool sort_by_window_start) {
            return RawAggregateIterator(path, sort_by_window_start);
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("sort_by_window_start") = false)
      .def_static(
          "raw_lines",
          [](const std::filesystem::path& path) {
            return RawLineIterator(path);
          },
          nb::arg("path"));

  currency_class.attr("Quote") = module.attr(quote_api_name);
  currency_class.attr("Aggregate") = module.attr(aggregate_api_name);
}

inline void export_backend_specific_aliases(nb::module_& m, nb::module_& flatfiles, nb::module_& websocket, const char* alias_prefix) {
  const std::string prefix = alias_prefix;

  m.attr((prefix + "FlatFileStocksParser").c_str()) = flatfiles.attr("Stock");
  m.attr((prefix + "FlatFileOptionsParser").c_str()) = flatfiles.attr("Options");
  m.attr((prefix + "FlatFileFuturesParser").c_str()) = flatfiles.attr("Futures");
  m.attr((prefix + "FlatFileIndicesParser").c_str()) = flatfiles.attr("Indices");
  m.attr((prefix + "FlatFileForexParser").c_str()) = flatfiles.attr("Forex");
  m.attr((prefix + "FlatFileCurrenciesParser").c_str()) = flatfiles.attr("currency");
  m.attr((prefix + "FlatFileCryptoParser").c_str()) = flatfiles.attr("Crypto");

  m.attr((prefix + "WebSocketMessagesParser").c_str()) = websocket.attr("Messages");
  m.attr((prefix + "WebSocketStocksParser").c_str()) = websocket.attr("Stocks");
  m.attr((prefix + "WebSocketOptionsParser").c_str()) = websocket.attr("Options");
  m.attr((prefix + "WebSocketFuturesParser").c_str()) = websocket.attr("Futures");
  m.attr((prefix + "WebSocketIndicesParser").c_str()) = websocket.attr("Indices");
  m.attr((prefix + "WebSocketForexParser").c_str()) = websocket.attr("Forex");
  m.attr((prefix + "WebSocketCryptoParser").c_str()) = websocket.attr("Crypto");
}

template <template <typename> class Impl>
void bind_backend_module(nb::module_& m, BackendKind kind, const char* alias_prefix) {
  m.doc() = "Backend-specialized parser bindings.";
  const std::string gzip_iterator_name = std::string(alias_prefix) + "GzipLinesIterator";
  const std::string stock_trade_iterator_name =
      std::string(alias_prefix) + "StockTradeRowsIterator";
  const std::string stock_quote_iterator_name =
      std::string(alias_prefix) + "StockQuoteRowsIterator";
  const std::string stock_aggregate_iterator_name =
      std::string(alias_prefix) + "StockAggregateRowsIterator";
  const std::string raw_stock_trade_iterator_name =
      std::string(alias_prefix) + "RawStockTradeRowsIterator";
  const std::string raw_stock_quote_iterator_name =
      std::string(alias_prefix) + "RawStockQuoteRowsIterator";
  const std::string raw_stock_aggregate_iterator_name =
      std::string(alias_prefix) + "RawStockAggregateRowsIterator";
  const std::string currency_quote_iterator_name =
      std::string(alias_prefix) + "CurrencyQuoteRowsIterator";
  const std::string raw_currency_quote_iterator_name =
      std::string(alias_prefix) + "RawCurrencyQuoteRowsIterator";
  const std::string currency_aggregate_iterator_name =
      std::string(alias_prefix) + "CurrencyAggregateRowsIterator";
  const std::string raw_currency_aggregate_iterator_name =
      std::string(alias_prefix) + "RawCurrencyAggregateRowsIterator";
  const std::string raw_line_iterator_name =
      std::string(alias_prefix) + "RawLineRowsIterator";
  const std::string stock_trade_api_name =
      std::string(alias_prefix) + "StockTradeApi";
  const std::string stock_quote_api_name =
      std::string(alias_prefix) + "StockQuoteApi";
  const std::string stock_aggregate_api_name =
      std::string(alias_prefix) + "StockAggregateApi";
  const std::string currency_quote_api_name =
      std::string(alias_prefix) + "CurrencyQuoteApi";
  const std::string currency_aggregate_api_name =
      std::string(alias_prefix) + "CurrencyAggregateApi";

  m.def(
      "read_gzip_lines_bytes",
      &read_gzip_lines_bytes<Impl<FlatFileStocksParser>>,
      nb::arg("path"),
      nb::arg("parallelization") = 0,
      nb::arg("chunk_size") = 1U << 20);
  m.def("detect_best_backend", &detect_best_backend);
  m.def("detect_processor_type", &detect_processor_type);
  m.def("backend_is_available", &backend_is_available, nb::arg("kind"));
  m.attr("BACKEND_KIND") = nb::cast(kind);
  m.attr("BACKEND_NAME") = nb::cast(std::string(backend_kind_name(kind)));
  m.attr("BACKEND_ALIAS_PREFIX") = nb::cast(std::string(alias_prefix));

  bind_gzip_lines<Impl<FlatFileStocksParser>>(m, gzip_iterator_name.c_str());
  bind_common_bases(m);

  auto flatfiles = m.def_submodule("FlatFiles", "Flat-file parser classes.");
  bind_stock_flatfile_asset<FlatFileStocksParser, Impl<FlatFileStocksParser>>(
      flatfiles,
      "Stock",
      stock_trade_iterator_name.c_str(),
      stock_quote_iterator_name.c_str(),
      stock_aggregate_iterator_name.c_str(),
      raw_stock_trade_iterator_name.c_str(),
      raw_stock_quote_iterator_name.c_str(),
      raw_stock_aggregate_iterator_name.c_str(),
      raw_line_iterator_name.c_str(),
      stock_trade_api_name.c_str(),
      stock_quote_api_name.c_str(),
      stock_aggregate_api_name.c_str());
  bind_flatfile_asset<FlatFileOptionsParser, Impl<FlatFileOptionsParser>>(flatfiles, "Options");
  bind_flatfile_asset<FlatFileFuturesParser, Impl<FlatFileFuturesParser>>(flatfiles, "Futures");
  bind_flatfile_asset<FlatFileIndicesParser, Impl<FlatFileIndicesParser>>(flatfiles, "Indices");
  bind_flatfile_asset<FlatFileForexParser, Impl<FlatFileForexParser>>(flatfiles, "Forex");
  bind_currency_flatfile_asset<FlatFileCurrenciesParser, Impl<FlatFileCurrenciesParser>>(
      flatfiles,
      "currency",
      currency_quote_iterator_name.c_str(),
      raw_currency_quote_iterator_name.c_str(),
      currency_aggregate_iterator_name.c_str(),
      raw_currency_aggregate_iterator_name.c_str(),
      raw_line_iterator_name.c_str(),
      currency_quote_api_name.c_str(),
      currency_aggregate_api_name.c_str());
  bind_flatfile_asset<FlatFileCryptoParser, Impl<FlatFileCryptoParser>>(flatfiles, "Crypto");

  auto websocket = m.def_submodule("WebSocket", "Websocket parser classes.");
  bind_websocket_asset<WebSocketMessagesParser, Impl<WebSocketMessagesParser>>(websocket, "Messages");
  bind_websocket_asset<WebSocketStocksParser, Impl<WebSocketStocksParser>>(websocket, "Stocks");
  bind_websocket_asset<WebSocketOptionsParser, Impl<WebSocketOptionsParser>>(websocket, "Options");
  bind_websocket_asset<WebSocketFuturesParser, Impl<WebSocketFuturesParser>>(websocket, "Futures");
  bind_websocket_asset<WebSocketIndicesParser, Impl<WebSocketIndicesParser>>(websocket, "Indices");
  bind_websocket_asset<WebSocketForexParser, Impl<WebSocketForexParser>>(websocket, "Forex");
  bind_websocket_asset<WebSocketCryptoParser, Impl<WebSocketCryptoParser>>(websocket, "Crypto");

  bind_row_models<typename Impl<FlatFileStocksParser>::specialization_type>(m, flatfiles);
  export_backend_specific_aliases(m, flatfiles, websocket, alias_prefix);
}

}  // namespace massive_speedup::bindings
