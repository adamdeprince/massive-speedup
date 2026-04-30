#pragma once

#include <nanobind/nanobind.h>
#include <nanobind/make_iterator.h>
#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/unordered_map.h>
#include <nanobind/stl/vector.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "native.hpp"
#include "massive_speedup/parsers.hpp"

namespace massive_speedup::bindings {

namespace nb = nanobind;

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

inline std::string_view bytes_view(nb::bytes value) {
  char* buffer = nullptr;
  Py_ssize_t length = 0;
  if (PyBytes_AsStringAndSize(value.ptr(), &buffer, &length) != 0) {
    throw nb::python_error();
  }
  return {buffer, static_cast<std::size_t>(length)};
}

template <typename RowType>
void construct_row_from_packed_with_ticker(
    RowType* self,
    nb::bytes packed,
    const std::string& ticker) {
  new (self) RowType(bytes_view(packed), ticker);
}

template <typename RowType>
RowType row_from_packed(nb::bytes packed, const std::string& ticker) {
  return RowType::from_packed(bytes_view(packed), ticker);
}

template <typename RowType>
std::uint64_t participant_timestamp_from_packed(nb::bytes packed) {
  const auto view = bytes_view(packed);
  native::detail::require_packed_size("packed row", view.size(), RowType::packed_size);
  return RowType::participant_timestamp_at(view.data());
}

template <typename RowType>
std::uint64_t sip_timestamp_from_packed(nb::bytes packed) {
  const auto view = bytes_view(packed);
  native::detail::require_packed_size("packed row", view.size(), RowType::packed_size);
  return RowType::sip_timestamp_at(view.data());
}

template <typename Specialization>
inline void bind_row_models(nb::module_& m, nb::module_& flatfiles) {
  auto stock_trade =
      nb::class_<native::StockTrade>(m, "StockTrade")
          .def(
              "__init__",
              [](native::StockTrade* self,
                 const std::vector<std::string>& fields) {
                new (self) native::StockTrade(
                    native::StockTrade::template from_fields<Specialization>(fields));
              },
              nb::arg("fields"))
          .def(
              "__init__",
              &construct_row_from_packed_with_ticker<native::StockTrade>,
              nb::arg("packed"),
              nb::arg("ticker"))
          .def_ro("ticker", &native::StockTrade::ticker)
          .def_prop_ro("conditions", &native::StockTrade::conditions_object)
          .def_ro("correction", &native::StockTrade::correction)
          .def_ro("exchange", &native::StockTrade::exchange)
          .def_ro("id", &native::StockTrade::id)
          .def_ro(
              "participant_timestamp",
              &native::StockTrade::participant_timestamp)
          .def_ro("price", &native::StockTrade::price)
          .def_ro(
              "sequence_number",
              &native::StockTrade::sequence_number)
          .def_ro("sip_timestamp", &native::StockTrade::sip_timestamp)
          .def_ro("size", &native::StockTrade::size)
          .def_ro("tape", &native::StockTrade::tape)
          .def_ro("trf_id", &native::StockTrade::trf_id)
          .def_ro("trf_timestamp", &native::StockTrade::trf_timestamp)
          .def(
              "__iter__",
              [](const native::StockTrade& self) {
                return native::detail::tuple_iterator(self.python_fields());
              })
          .def("pack", &native::StockTrade::packed_bytes)
          .def_static(
              "from_packed",
              &row_from_packed<native::StockTrade>,
              nb::arg("packed"),
              nb::arg("ticker"))
          .def_static(
              "participant_timestamp_from_packed",
              &participant_timestamp_from_packed<native::StockTrade>,
              nb::arg("packed"))
          .def_static(
              "sip_timestamp_from_packed",
              &sip_timestamp_from_packed<native::StockTrade>,
              nb::arg("packed"))
          .def("__hash__", &native::StockTrade::hash_value)
          .def("__str__", &native::StockTrade::repr)
          .def("__repr__", &native::StockTrade::repr)
          .def(
              "__eq__",
              [](const native::StockTrade& lhs,
                 const native::StockTrade& rhs) { return lhs == rhs; },
              nb::is_operator());
  stock_trade.attr("packed_size") = nb::int_(native::StockTrade::packed_size);
  stock_trade.attr("packed_participant_timestamp_offset") =
      nb::int_(native::StockTrade::packed_participant_timestamp_offset);
  stock_trade.attr("packed_sip_timestamp_offset") =
      nb::int_(native::StockTrade::packed_sip_timestamp_offset);
  bind_participant_timestamp_ordering(stock_trade);

  auto stock_quote =
      nb::class_<native::StockQuote>(m, "StockQuote")
          .def(
              "__init__",
              [](native::StockQuote* self,
                 const std::vector<std::string>& fields) {
                new (self) native::StockQuote(
                    native::StockQuote::template from_fields<Specialization>(fields));
              },
              nb::arg("fields"))
          .def(
              "__init__",
              &construct_row_from_packed_with_ticker<native::StockQuote>,
              nb::arg("packed"),
              nb::arg("ticker"))
          .def_ro("ticker", &native::StockQuote::ticker)
          .def_ro("ask_exchange", &native::StockQuote::ask_exchange)
          .def_ro("ask_price", &native::StockQuote::ask_price)
          .def_ro("ask_size", &native::StockQuote::ask_size)
          .def_ro("bid_exchange", &native::StockQuote::bid_exchange)
          .def_ro("bid_price", &native::StockQuote::bid_price)
          .def_ro("bid_size", &native::StockQuote::bid_size)
          .def_prop_ro("conditions", &native::StockQuote::conditions_object)
          .def_prop_ro("indicators", &native::StockQuote::indicators_object)
          .def_ro(
              "participant_timestamp",
              &native::StockQuote::participant_timestamp)
          .def_ro(
              "sequence_number",
              &native::StockQuote::sequence_number)
          .def_ro("sip_timestamp", &native::StockQuote::sip_timestamp)
          .def_ro("tape", &native::StockQuote::tape)
          .def_ro("trf_timestamp", &native::StockQuote::trf_timestamp)
          .def(
              "__iter__",
              [](const native::StockQuote& self) {
                return native::detail::tuple_iterator(self.python_fields());
              })
          .def("pack", &native::StockQuote::packed_bytes)
          .def_static(
              "from_packed",
              &row_from_packed<native::StockQuote>,
              nb::arg("packed"),
              nb::arg("ticker"))
          .def_static(
              "participant_timestamp_from_packed",
              &participant_timestamp_from_packed<native::StockQuote>,
              nb::arg("packed"))
          .def_static(
              "sip_timestamp_from_packed",
              &sip_timestamp_from_packed<native::StockQuote>,
              nb::arg("packed"))
          .def("__hash__", &native::StockQuote::hash_value)
          .def("__str__", &native::StockQuote::repr)
          .def("__repr__", &native::StockQuote::repr)
          .def(
              "__eq__",
              [](const native::StockQuote& lhs,
                 const native::StockQuote& rhs) { return lhs == rhs; },
              nb::is_operator());
  stock_quote.attr("packed_size") = nb::int_(native::StockQuote::packed_size);
  stock_quote.attr("packed_participant_timestamp_offset") =
      nb::int_(native::StockQuote::packed_participant_timestamp_offset);
  stock_quote.attr("packed_sip_timestamp_offset") =
      nb::int_(native::StockQuote::packed_sip_timestamp_offset);
  bind_participant_timestamp_ordering(stock_quote);

  auto currency_quote =
      nb::class_<native::CurrencyQuote>(m, "CurrencyQuote")
          .def(
              "__init__",
              [](native::CurrencyQuote* self,
                 const std::vector<std::string>& fields) {
                new (self) native::CurrencyQuote(
                    native::CurrencyQuote::template from_fields<Specialization>(fields));
              },
              nb::arg("fields"))
          .def(
              "__init__",
              &construct_row_from_packed_with_ticker<native::CurrencyQuote>,
              nb::arg("packed"),
              nb::arg("ticker"))
          .def_ro("ticker", &native::CurrencyQuote::ticker)
          .def_ro("ask_exchange", &native::CurrencyQuote::ask_exchange)
          .def_ro("ask_price", &native::CurrencyQuote::ask_price)
          .def_ro("bid_exchange", &native::CurrencyQuote::bid_exchange)
          .def_ro("bid_price", &native::CurrencyQuote::bid_price)
          .def_prop_ro("tickers", &native::CurrencyQuote::tickers_object)
          .def_ro(
              "participant_timestamp",
              &native::CurrencyQuote::participant_timestamp)
          .def(
              "__iter__",
              [](const native::CurrencyQuote& self) {
                return native::detail::tuple_iterator(self.python_fields());
              })
          .def("pack", &native::CurrencyQuote::packed_bytes)
          .def_static(
              "from_packed",
              &row_from_packed<native::CurrencyQuote>,
              nb::arg("packed"),
              nb::arg("ticker"))
          .def_static(
              "participant_timestamp_from_packed",
              &participant_timestamp_from_packed<native::CurrencyQuote>,
              nb::arg("packed"))
          .def("__hash__", &native::CurrencyQuote::hash_value)
          .def("__str__", &native::CurrencyQuote::repr)
          .def("__repr__", &native::CurrencyQuote::repr)
          .def(
              "__eq__",
              [](const native::CurrencyQuote& lhs,
                 const native::CurrencyQuote& rhs) { return lhs == rhs; },
              nb::is_operator());
  currency_quote.attr("packed_size") = nb::int_(native::CurrencyQuote::packed_size);
  currency_quote.attr("packed_participant_timestamp_offset") =
      nb::int_(native::CurrencyQuote::packed_participant_timestamp_offset);
  bind_participant_timestamp_ordering(currency_quote);

  auto stock_aggregate =
      nb::class_<native::StockAggregate>(m, "StockAggregate")
          .def(
              "__init__",
              [](native::StockAggregate* self,
                 const std::vector<std::string>& fields) {
                new (self) native::StockAggregate(
                    native::StockAggregate::template from_fields<Specialization>(fields));
              },
              nb::arg("fields"))
          .def(
              "__init__",
              &construct_row_from_packed_with_ticker<native::StockAggregate>,
              nb::arg("packed"),
              nb::arg("ticker"))
          .def_ro("ticker", &native::StockAggregate::ticker)
          .def_ro("volume", &native::StockAggregate::volume)
          .def_ro("open", &native::StockAggregate::open)
          .def_ro("close", &native::StockAggregate::close)
          .def_ro("high", &native::StockAggregate::high)
          .def_ro("low", &native::StockAggregate::low)
          .def_ro("window_start", &native::StockAggregate::window_start)
          .def_ro("transactions", &native::StockAggregate::transactions)
          .def(
              "__iter__",
              [](const native::StockAggregate& self) {
                return native::detail::tuple_iterator(self.python_fields());
              })
          .def("pack", &native::StockAggregate::packed_bytes)
          .def_static(
              "from_packed",
              &row_from_packed<native::StockAggregate>,
              nb::arg("packed"),
              nb::arg("ticker"))
          .def("__hash__", &native::StockAggregate::hash_value)
          .def("__str__", &native::StockAggregate::repr)
          .def("__repr__", &native::StockAggregate::repr)
          .def(
              "__eq__",
              [](const native::StockAggregate& lhs,
                 const native::StockAggregate& rhs) { return lhs == rhs; },
              nb::is_operator());
  stock_aggregate.attr("packed_size") = nb::int_(native::StockAggregate::packed_size);
  bind_window_start_ordering(stock_aggregate);

  auto currency_aggregate =
      nb::class_<native::CurrencyAggregate>(m, "CurrencyAggregate")
          .def(
              "__init__",
              [](native::CurrencyAggregate* self,
                 const std::vector<std::string>& fields) {
                new (self) native::CurrencyAggregate(
                    native::CurrencyAggregate::template from_fields<Specialization>(fields));
              },
              nb::arg("fields"))
          .def(
              "__init__",
              &construct_row_from_packed_with_ticker<native::CurrencyAggregate>,
              nb::arg("packed"),
              nb::arg("ticker"))
          .def_ro("ticker", &native::CurrencyAggregate::ticker)
          .def_ro("volume", &native::CurrencyAggregate::volume)
          .def_ro("open", &native::CurrencyAggregate::open)
          .def_ro("close", &native::CurrencyAggregate::close)
          .def_ro("high", &native::CurrencyAggregate::high)
          .def_ro("low", &native::CurrencyAggregate::low)
          .def_ro("window_start", &native::CurrencyAggregate::window_start)
          .def_ro("transactions", &native::CurrencyAggregate::transactions)
          .def_prop_ro("tickers", &native::CurrencyAggregate::tickers_object)
          .def(
              "__iter__",
              [](const native::CurrencyAggregate& self) {
                return native::detail::tuple_iterator(self.python_fields());
              })
          .def("pack", &native::CurrencyAggregate::packed_bytes)
          .def_static(
              "from_packed",
              &row_from_packed<native::CurrencyAggregate>,
              nb::arg("packed"),
              nb::arg("ticker"))
          .def("__hash__", &native::CurrencyAggregate::hash_value)
          .def("__str__", &native::CurrencyAggregate::repr)
          .def("__repr__", &native::CurrencyAggregate::repr)
          .def(
              "__eq__",
              [](const native::CurrencyAggregate& lhs,
                 const native::CurrencyAggregate& rhs) { return lhs == rhs; },
              nb::is_operator());
  currency_aggregate.attr("packed_size") = nb::int_(native::CurrencyAggregate::packed_size);
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
      .def_static("processor_name", &processor_name_static<ImplAsset>);
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
      .def_static("processor_name", &processor_name_static<ImplAsset>);

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
      .def_static("processor_name", &processor_name_static<ImplAsset>);
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
      .def_static("processor_name", &processor_name_static<ImplAsset>);

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

template <template <typename> class Impl>
void bind_native_module(nb::module_& m, const char* alias_prefix) {
  m.doc() = "Native parser bindings.";
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
  m.def(
      "build_database_file",
      [](const std::filesystem::path& input_path,
         const std::filesystem::path& database_path,
         const std::string& record_type) {
        return Impl<FlatFileStocksParser>::build_database_file(
            input_path,
            database_path,
            record_type);
      },
      nb::arg("input_path"),
      nb::arg("database_path"),
      nb::arg("record_type"),
      nb::call_guard<nb::gil_scoped_release>());
  static_cast<void>(alias_prefix);

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
}

}  // namespace massive_speedup::bindings
