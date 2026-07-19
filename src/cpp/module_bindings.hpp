#pragma once

#include <nanobind/nanobind.h>
#include <nanobind/make_iterator.h>
#include <nanobind/stl/filesystem.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/unordered_map.h>
#include <nanobind/stl/vector.h>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <sstream>
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
using CryptoTradeRowsIterator = typename ParserType::CryptoTradeRowsIterator;

template <typename ParserType>
using OptionTradeRowsIterator = typename ParserType::OptionTradeRowsIterator;

template <typename ParserType>
using OptionQuoteRowsIterator = typename ParserType::OptionQuoteRowsIterator;

template <typename ParserType>
using StockQuoteRowsIterator = typename ParserType::StockQuoteRowsIterator;

template <typename ParserType>
using FuturesTradeRowsIterator = typename ParserType::FuturesTradeRowsIterator;

template <typename ParserType>
using FuturesQuoteRowsIterator = typename ParserType::FuturesQuoteRowsIterator;

template <typename ParserType>
using StockAggregateRowsIterator = typename ParserType::StockAggregateRowsIterator;

template <typename ParserType>
using RawStockTradeRowsIterator = typename ParserType::RawStockTradeRowsIterator;

template <typename ParserType>
using RawCryptoTradeRowsIterator = typename ParserType::RawCryptoTradeRowsIterator;

template <typename ParserType>
using RawOptionTradeRowsIterator = typename ParserType::RawOptionTradeRowsIterator;

template <typename ParserType>
using RawOptionQuoteRowsIterator = typename ParserType::RawOptionQuoteRowsIterator;

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
struct CryptoTradeApi {};

template <typename ParserType>
struct OptionTradeApi {};

template <typename ParserType>
struct OptionQuoteApi {};

template <typename ParserType>
struct StockQuoteApi {};

template <typename ParserType>
struct StockAggregateApi {};

template <typename ParserType>
struct FuturesTradeApi {};

template <typename ParserType>
struct FuturesQuoteApi {};

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

inline nb::object bind_condition_enum(
    nb::module_& m,
    const char* class_name,
    native::detail::ConditionSetKind kind) {
  nb::dict members;
  const auto& metadata = native::detail::condition_metadata_table(kind);
  for (std::size_t index = 0; index < metadata.size(); ++index) {
    if (!metadata[index].has_value()) {
      continue;
    }
    members[nb::str(metadata[index]->enum_name.data())] = nb::int_(index);
  }

  nb::module_ condition_enums =
      nb::module_::import_("massive_speedup._condition_enums");
  nb::object enum_type =
      condition_enums.attr("make_condition_enum")(class_name, members);
  m.attr(class_name) = enum_type;
  native::detail::install_condition_enum_members(kind, enum_type);
  return enum_type;
}

inline void bind_condition_enums(nb::module_& m) {
  bind_condition_enum(
      m,
      "StockTradeCondition",
      native::detail::ConditionSetKind::stock_trade);
  bind_condition_enum(
      m,
      "StockQuoteCondition",
      native::detail::ConditionSetKind::stock_quote);
}

inline std::string date_argument_to_string(nb::handle date) {
  if (PyUnicode_Check(date.ptr())) {
    return nb::cast<std::string>(date);
  }

  if (!PyObject_HasAttrString(date.ptr(), "isoformat")) {
    throw std::invalid_argument("date must be an ISO date string or datetime.date");
  }

  nb::object isoformat = nb::borrow<nb::object>(date).attr("isoformat")();
  return nb::cast<std::string>(isoformat);
}

inline std::uint64_t nanoseconds_argument_to_uint64(
    nb::handle value,
    const char* argument_name) {
  if (PyLong_Check(value.ptr())) {
    return nb::cast<std::uint64_t>(value);
  }

  if (PyFloat_Check(value.ptr())) {
    const double parsed = PyFloat_AsDouble(value.ptr());
    if (PyErr_Occurred()) {
      throw nb::python_error();
    }

    const long double parsed_value = static_cast<long double>(parsed);
    const long double maximum_value =
        static_cast<long double>(std::numeric_limits<std::uint64_t>::max());
    if (!std::isfinite(parsed) || parsed_value < 0 || parsed_value > maximum_value) {
      std::ostringstream message;
      message << argument_name << " must be finite non-negative nanoseconds";
      throw std::invalid_argument(message.str());
    }

    return static_cast<std::uint64_t>(parsed);
  }

  std::ostringstream message;
  message << argument_name << " must be integer or float nanoseconds";
  throw std::invalid_argument(message.str());
}

inline std::optional<std::int64_t> galloping_argument_to_optional_index(
    nb::handle value) {
  if (value.ptr() == Py_None) {
    return std::nullopt;
  }

  PyObject* index_object_ptr = PyNumber_Index(value.ptr());
  if (index_object_ptr == nullptr) {
    PyErr_Clear();
    throw std::invalid_argument("galloping must be None or an integer index");
  }
  nb::object index_object = nb::steal<nb::object>(index_object_ptr);

  int overflow = 0;
  const long long parsed =
      PyLong_AsLongLongAndOverflow(index_object.ptr(), &overflow);
  if (overflow > 0) {
    return std::numeric_limits<std::int64_t>::max();
  }
  if (overflow < 0) {
    return std::numeric_limits<std::int64_t>::min();
  }
  if (PyErr_Occurred()) {
    throw nb::python_error();
  }

  return static_cast<std::int64_t>(parsed);
}

template <typename DatabaseType>
std::uint64_t timestamp_argument_to_ns(
    const DatabaseType& database,
    nb::handle timestamp) {
  if (PyLong_Check(timestamp.ptr()) || PyFloat_Check(timestamp.ptr())) {
    return nanoseconds_argument_to_uint64(timestamp, "timestamp");
  }

  if (!PyObject_HasAttrString(timestamp.ptr(), "hour") ||
      !PyObject_HasAttrString(timestamp.ptr(), "minute") ||
      !PyObject_HasAttrString(timestamp.ptr(), "second")) {
    throw std::invalid_argument(
        "timestamp must be integer/float nanoseconds or datetime.time");
  }

  nb::module_ datetime_module = nb::module_::import_("datetime");
  nb::object timezone = datetime_module.attr("timezone");
  nb::object time_object = nb::borrow<nb::object>(timestamp);
  nb::object tzinfo = time_object.attr("tzinfo");
  if (tzinfo.ptr() == Py_None) {
    time_object = time_object.attr("replace")(nb::arg("tzinfo") = timezone.attr("utc"));
  }

  nb::object date_object =
      datetime_module.attr("date").attr("fromisoformat")(database.date());
  nb::object datetime_object =
      datetime_module.attr("datetime").attr("combine")(date_object, time_object);
  nb::object epoch = datetime_module.attr("datetime")(
      1970,
      1,
      1,
      nb::arg("tzinfo") = timezone.attr("utc"));

  PyObject* delta_ptr = PyNumber_Subtract(datetime_object.ptr(), epoch.ptr());
  if (delta_ptr == nullptr) {
    throw nb::python_error();
  }
  nb::object delta = nb::steal<nb::object>(delta_ptr);

  const auto days = nb::cast<long long>(delta.attr("days"));
  const auto seconds = nb::cast<long long>(delta.attr("seconds"));
  const auto microseconds = nb::cast<long long>(delta.attr("microseconds"));
  const __int128 total_ns =
      static_cast<__int128>(days) * 86'400'000'000'000LL +
      static_cast<__int128>(seconds) * 1'000'000'000LL +
      static_cast<__int128>(microseconds) * 1'000LL;
  if (total_ns < 0) {
    throw std::invalid_argument("datetime.time resolved to a timestamp before epoch");
  }
  if (total_ns > static_cast<__int128>(std::numeric_limits<std::uint64_t>::max())) {
    throw std::invalid_argument("datetime.time resolved to a timestamp out of range");
  }
  return static_cast<std::uint64_t>(total_ns);
}

template <typename DatabaseType>
void bind_database_record_file(
    nb::module_& m,
    const char* name,
    const char* iterator_name) {
  using IteratorType = typename DatabaseType::Iterator;

  bind_iterator_type<IteratorType>(m, iterator_name);

  auto class_ = nb::class_<DatabaseType>(m, name)
      .def(
          "__init__",
          [](DatabaseType* self,
             const std::filesystem::path& database_path,
             nb::handle date,
             const std::string& ticker) {
            new (self) DatabaseType(database_path, date_argument_to_string(date), ticker);
          },
          nb::arg("database_path"),
          nb::arg("date"),
          nb::arg("ticker"))
      .def_prop_ro("ticker", &DatabaseType::ticker)
      .def_prop_ro("date", &DatabaseType::date)
      .def_prop_ro("record_type", &DatabaseType::record_type)
      .def_prop_ro("database_path", &DatabaseType::database_path)
      .def_prop_ro("path", &DatabaseType::path)
      .def("__len__", &DatabaseType::size)
      .def("__getitem__", &DatabaseType::get_item, nb::arg("index"))
      .def(
          "__iter__",
          [](const DatabaseType& self) { return self.iter(); },
          nb::keep_alive<0, 1>())
      .def(
          "index_before_timestamp",
          [](const DatabaseType& self, nb::handle timestamp, nb::handle galloping) {
            return self.index_before_timestamp(
                timestamp_argument_to_ns(self, timestamp),
                galloping_argument_to_optional_index(galloping));
          },
          nb::arg("timestamp"),
          nb::kw_only(),
          nb::arg("galloping") = nb::none())
      .def(
          "index_after_timestamp",
          [](const DatabaseType& self, nb::handle timestamp, nb::handle galloping) {
            return self.index_after_timestamp(
                timestamp_argument_to_ns(self, timestamp),
                galloping_argument_to_optional_index(galloping));
          },
          nb::arg("timestamp"),
          nb::kw_only(),
          nb::arg("galloping") = nb::none())
      .def(
          "find_before_participant_timestamp",
          [](const DatabaseType& self,
             nb::handle timestamp,
             nb::handle fuzz,
             nb::handle galloping,
             bool on) {
            return self.find_before_participant_timestamp(
                timestamp_argument_to_ns(self, timestamp),
                nanoseconds_argument_to_uint64(fuzz, "fuzz"),
                galloping_argument_to_optional_index(galloping),
                on);
          },
          nb::arg("timestamp"),
          nb::arg("fuzz") = 1'000'000'000ULL,
          nb::kw_only(),
          nb::arg("galloping") = nb::none(),
          nb::arg("on") = true)
      .def(
          "find_after_participant_timestamp",
          [](const DatabaseType& self,
             nb::handle timestamp,
             nb::handle fuzz,
             nb::handle galloping,
             bool on) {
            return self.find_after_participant_timestamp(
                timestamp_argument_to_ns(self, timestamp),
                nanoseconds_argument_to_uint64(fuzz, "fuzz"),
                galloping_argument_to_optional_index(galloping),
                on);
          },
          nb::arg("timestamp"),
          nb::arg("fuzz") = 1'000'000'000ULL,
          nb::kw_only(),
          nb::arg("galloping") = nb::none(),
          nb::arg("on") = true)
      .def(
          "iterate_bounded",
          [](const DatabaseType& self, nb::handle start_timestamp) {
            return self.iterate_bounded(timestamp_argument_to_ns(self, start_timestamp));
          },
          nb::arg("start_timestamp"),
          nb::keep_alive<0, 1>())
      .def(
          "iterate_bounded",
          [](const DatabaseType& self,
             nb::handle start_timestamp,
             nb::handle stop_timestamp) {
            return self.iterate_bounded(
                timestamp_argument_to_ns(self, start_timestamp),
                timestamp_argument_to_ns(self, stop_timestamp));
          },
          nb::arg("start_timestamp"),
          nb::arg("stop_timestamp"),
          nb::keep_alive<0, 1>());

  if constexpr (
      std::is_same_v<DatabaseType, native::StockTradeDatabase> ||
      std::is_same_v<DatabaseType, native::StockQuoteDatabase>) {
    class_
        .def_prop_ro("market_open", &DatabaseType::market_open)
        .def_prop_ro("market_close", &DatabaseType::market_close);
  }
}

template <typename DatabaseType>
void bind_futures_database_record_file(
    nb::module_& m,
    const char* name,
    const char* iterator_name) {
  using IteratorType = typename DatabaseType::Iterator;

  bind_iterator_type<IteratorType>(m, iterator_name);

  nb::class_<DatabaseType>(m, name)
      .def(
          "__init__",
          [](DatabaseType* self,
             const std::filesystem::path& database_path,
             nb::handle date,
             const std::string& ticker,
             const std::string& exchange) {
            new (self) DatabaseType(
                database_path,
                date_argument_to_string(date),
                ticker,
                exchange);
          },
          nb::arg("database_path"),
          nb::arg("date"),
          nb::arg("ticker"),
          nb::kw_only(),
          nb::arg("exchange") = "")
      .def_prop_ro("ticker", &DatabaseType::ticker)
      .def_prop_ro("date", &DatabaseType::date)
      .def_prop_ro("record_type", &DatabaseType::record_type)
      .def_prop_ro("database_path", &DatabaseType::database_path)
      .def_prop_ro("path", &DatabaseType::path)
      .def("__len__", &DatabaseType::size)
      .def("__getitem__", &DatabaseType::get_item, nb::arg("index"))
      .def(
          "__iter__",
          [](const DatabaseType& self) { return self.iter(); },
          nb::keep_alive<0, 1>())
      .def(
          "index_before_timestamp",
          [](const DatabaseType& self, nb::handle timestamp, nb::handle galloping) {
            return self.index_before_timestamp(
                timestamp_argument_to_ns(self, timestamp),
                galloping_argument_to_optional_index(galloping));
          },
          nb::arg("timestamp"),
          nb::kw_only(),
          nb::arg("galloping") = nb::none())
      .def(
          "index_after_timestamp",
          [](const DatabaseType& self, nb::handle timestamp, nb::handle galloping) {
            return self.index_after_timestamp(
                timestamp_argument_to_ns(self, timestamp),
                galloping_argument_to_optional_index(galloping));
          },
          nb::arg("timestamp"),
          nb::kw_only(),
          nb::arg("galloping") = nb::none())
      .def(
          "iterate_bounded",
          [](const DatabaseType& self, nb::handle start_timestamp) {
            return self.iterate_bounded(timestamp_argument_to_ns(self, start_timestamp));
          },
          nb::arg("start_timestamp"),
          nb::keep_alive<0, 1>())
      .def(
          "iterate_bounded",
          [](const DatabaseType& self,
             nb::handle start_timestamp,
             nb::handle stop_timestamp) {
            return self.iterate_bounded(
                timestamp_argument_to_ns(self, start_timestamp),
                timestamp_argument_to_ns(self, stop_timestamp));
          },
          nb::arg("start_timestamp"),
          nb::arg("stop_timestamp"),
          nb::keep_alive<0, 1>());
}

template <typename DatabaseType>
void bind_option_database_record_file(
    nb::module_& m,
    const char* name,
    const char* iterator_name) {
  using IteratorType = typename DatabaseType::Iterator;

  bind_iterator_type<IteratorType>(m, iterator_name);

  nb::class_<DatabaseType>(m, name)
      .def(
          "__init__",
          [](DatabaseType* self,
             const std::filesystem::path& database_path,
             nb::handle date,
             const std::string& root,
             const std::string& expiration,
             const std::string& right,
             double strike) {
            new (self) DatabaseType(
                database_path,
                date_argument_to_string(date),
                root,
                expiration,
                right,
                strike);
          },
          nb::arg("database_path"),
          nb::arg("date"),
          nb::arg("root"),
          nb::arg("expiration"),
          nb::arg("right"),
          nb::arg("strike"))
      .def_prop_ro("root", &DatabaseType::root)
      .def_prop_ro("expiration", &DatabaseType::expiration)
      .def_prop_ro("right", &DatabaseType::right)
      .def_prop_ro("strike", &DatabaseType::strike)
      .def_prop_ro("strike_millis", &DatabaseType::strike_millis)
      .def_prop_ro("contract_key", &DatabaseType::contract_key)
      .def_prop_ro("date", &DatabaseType::date)
      .def_prop_ro("record_type", &DatabaseType::record_type)
      .def_prop_ro("database_path", &DatabaseType::database_path)
      .def_prop_ro("path", &DatabaseType::path)
      .def("__len__", &DatabaseType::size)
      .def("__getitem__", &DatabaseType::get_item, nb::arg("index"))
      .def(
          "__iter__",
          [](const DatabaseType& self) { return self.iter(); },
          nb::keep_alive<0, 1>())
      .def(
          "index_before_timestamp",
          [](const DatabaseType& self, nb::handle timestamp, nb::handle galloping) {
            return self.index_before_timestamp(
                timestamp_argument_to_ns(self, timestamp),
                galloping_argument_to_optional_index(galloping));
          },
          nb::arg("timestamp"),
          nb::kw_only(),
          nb::arg("galloping") = nb::none())
      .def(
          "index_after_timestamp",
          [](const DatabaseType& self, nb::handle timestamp, nb::handle galloping) {
            return self.index_after_timestamp(
                timestamp_argument_to_ns(self, timestamp),
                galloping_argument_to_optional_index(galloping));
          },
          nb::arg("timestamp"),
          nb::kw_only(),
          nb::arg("galloping") = nb::none())
      .def(
          "iterate_bounded",
          [](const DatabaseType& self, nb::handle start_timestamp) {
            return self.iterate_bounded(timestamp_argument_to_ns(self, start_timestamp));
          },
          nb::arg("start_timestamp"),
          nb::keep_alive<0, 1>())
      .def(
          "iterate_bounded",
          [](const DatabaseType& self,
             nb::handle start_timestamp,
             nb::handle stop_timestamp) {
            return self.iterate_bounded(
                timestamp_argument_to_ns(self, start_timestamp),
                timestamp_argument_to_ns(self, stop_timestamp));
          },
          nb::arg("start_timestamp"),
          nb::arg("stop_timestamp"),
          nb::keep_alive<0, 1>());
}

inline void bind_stock_trade_quote_timeline(nb::module_& m) {
  nb::class_<native::StockTradeQuoteTimeline>(m, "StockTradeQuoteTimeline")
      .def(
          "__init__",
          [](native::StockTradeQuoteTimeline* self,
             const std::filesystem::path& database_path,
             nb::handle date,
             const std::string& ticker) {
            new (self) native::StockTradeQuoteTimeline(
                database_path,
                date_argument_to_string(date),
                ticker);
          },
          nb::arg("database_path"),
          nb::arg("date"),
          nb::arg("ticker"))
      .def(
          "__iter__",
          [](native::StockTradeQuoteTimeline& self)
              -> native::StockTradeQuoteTimeline& { return self.iter(); },
          nb::rv_policy::reference_internal)
      .def("__next__", &native::StockTradeQuoteTimeline::next);

  m.def(
      "stock_trade_quote_timeline",
      [](const std::filesystem::path& database_path,
         nb::handle date,
         const std::string& ticker) {
        return native::StockTradeQuoteTimeline(
            database_path,
            date_argument_to_string(date),
            ticker);
      },
      nb::arg("database_path"),
      nb::arg("date"),
      nb::arg("ticker"));
}

inline void bind_simple_market(nb::module_& m) {
  nb::class_<native::SimpleMarketBroker>(m, "SimpleMarketBroker")
      .def_prop_ro("symbol", &native::SimpleMarketBroker::symbol)
      .def_prop_ro("sip_timestamp", &native::SimpleMarketBroker::sip_timestamp)
      .def(
          "buy",
          [](native::SimpleMarketBroker& self, double shares, nb::handle symbol) {
            if (symbol.is_none()) {
              self.buy(shares);
              return;
            }
            self.buy(shares, nb::cast<std::string>(symbol));
          },
          nb::arg("shares"),
          nb::arg("symbol") = nb::none())
      .def(
          "sell",
          [](native::SimpleMarketBroker& self, double shares, nb::handle symbol) {
            if (symbol.is_none()) {
              self.sell(shares);
              return;
            }
            self.sell(shares, nb::cast<std::string>(symbol));
          },
          nb::arg("shares"),
          nb::arg("symbol") = nb::none());

  nb::class_<native::SimpleMarket>(m, "SimpleMarket")
      .def(
          "__init__",
          [](native::SimpleMarket* self,
             const std::filesystem::path& database_path,
             nb::handle date,
             const std::vector<std::string>& symbols,
             std::uint64_t trade_latency_ns,
             bool quotes,
             bool fast) {
            new (self) native::SimpleMarket(
                database_path,
                date_argument_to_string(date),
                symbols,
                trade_latency_ns,
                quotes,
                fast);
          },
          nb::arg("database_path"),
          nb::arg("date"),
          nb::arg("symbols"),
          nb::arg("trade_latency_ns"),
          nb::kw_only(),
          nb::arg("quotes") = false,
          nb::arg("fast") = false)
      .def(
          "__iter__",
          [](native::SimpleMarket& self) -> native::SimpleMarket& {
            return self.iter();
          },
          nb::rv_policy::reference_internal)
      .def("__next__", &native::SimpleMarket::next)
      .def(
          "__getitem__",
          [](const native::SimpleMarket& self, nb::handle key) {
            return self.get_holding(key);
          },
          nb::arg("key").none())
      .def(
          "__contains__",
          [](const native::SimpleMarket& self, nb::handle key) {
            return self.contains(key);
          },
          nb::arg("key").none())
      .def("__len__", &native::SimpleMarket::size)
      .def_prop_ro("broker", &native::SimpleMarket::broker)
      .def("keys", &native::SimpleMarket::keys)
      .def("values", &native::SimpleMarket::values)
      .def("items", &native::SimpleMarket::items)
      .def("as_dict", &native::SimpleMarket::as_dict);

  nb::class_<native::FuturesMarketBroker>(m, "FuturesMarketBroker")
      .def_prop_ro("symbol", &native::FuturesMarketBroker::symbol)
      .def_prop_ro("timestamp", &native::FuturesMarketBroker::timestamp)
      .def_prop_ro("sip_timestamp", &native::FuturesMarketBroker::sip_timestamp)
      .def(
          "buy",
          [](native::FuturesMarketBroker& self, double contracts, nb::handle symbol) {
            if (symbol.is_none()) {
              self.buy(contracts);
              return;
            }
            self.buy(contracts, nb::cast<std::string>(symbol));
          },
          nb::arg("contracts"),
          nb::arg("symbol") = nb::none())
      .def(
          "sell",
          [](native::FuturesMarketBroker& self, double contracts, nb::handle symbol) {
            if (symbol.is_none()) {
              self.sell(contracts);
              return;
            }
            self.sell(contracts, nb::cast<std::string>(symbol));
          },
          nb::arg("contracts"),
          nb::arg("symbol") = nb::none());

  nb::class_<native::FuturesMarket>(m, "FuturesMarket")
      .def(
          "__init__",
          [](native::FuturesMarket* self,
             const std::filesystem::path& database_path,
             nb::handle date,
             const std::vector<std::string>& symbols,
             std::uint64_t trade_latency_ns,
             const std::string& exchange,
             bool quotes,
             bool fast) {
            new (self) native::FuturesMarket(
                database_path,
                date_argument_to_string(date),
                symbols,
                trade_latency_ns,
                exchange,
                quotes,
                fast);
          },
          nb::arg("database_path"),
          nb::arg("date"),
          nb::arg("symbols"),
          nb::arg("trade_latency_ns"),
          nb::kw_only(),
          nb::arg("exchange") = "",
          nb::arg("quotes") = false,
          nb::arg("fast") = false)
      .def(
          "__iter__",
          [](native::FuturesMarket& self) -> native::FuturesMarket& {
            return self.iter();
          },
          nb::rv_policy::reference_internal)
      .def("__next__", &native::FuturesMarket::next)
      .def(
          "__getitem__",
          [](const native::FuturesMarket& self, nb::handle key) {
            return self.get_holding(key);
          },
          nb::arg("key").none())
      .def(
          "__contains__",
          [](const native::FuturesMarket& self, nb::handle key) {
            return self.contains(key);
          },
          nb::arg("key").none())
      .def("__len__", &native::FuturesMarket::size)
      .def_prop_ro("broker", &native::FuturesMarket::broker)
      .def("keys", &native::FuturesMarket::keys)
      .def("values", &native::FuturesMarket::values)
      .def("items", &native::FuturesMarket::items)
      .def("as_dict", &native::FuturesMarket::as_dict);

  nb::class_<native::OptionMarketBroker>(m, "OptionMarketBroker")
      .def_prop_ro("contract", &native::OptionMarketBroker::contract)
      .def_prop_ro("sip_timestamp", &native::OptionMarketBroker::sip_timestamp)
      .def("buy", &native::OptionMarketBroker::buy, nb::arg("contracts"))
      .def("sell", &native::OptionMarketBroker::sell, nb::arg("contracts"));

  nb::class_<native::OptionMarket>(m, "OptionMarket")
      .def(
          "__init__",
          [](native::OptionMarket* self,
             const std::filesystem::path& database_path,
             nb::handle date,
             const std::string& root,
             const std::string& expiration,
             const std::string& right,
             double strike,
             std::uint64_t trade_latency_ns,
             bool quotes,
             bool fast) {
            new (self) native::OptionMarket(
                database_path,
                date_argument_to_string(date),
                root,
                expiration,
                right,
                strike,
                trade_latency_ns,
                quotes,
                fast);
          },
          nb::arg("database_path"),
          nb::arg("date"),
          nb::arg("root"),
          nb::arg("expiration"),
          nb::arg("right"),
          nb::arg("strike"),
          nb::arg("trade_latency_ns"),
          nb::kw_only(),
          nb::arg("quotes") = false,
          nb::arg("fast") = false)
      .def(
          "__iter__",
          [](native::OptionMarket& self) -> native::OptionMarket& {
            return self.iter();
          },
          nb::rv_policy::reference_internal)
      .def("__next__", &native::OptionMarket::next)
      .def(
          "__getitem__",
          [](const native::OptionMarket& self, nb::handle key) {
            return self.get_holding(key);
          },
          nb::arg("key").none())
      .def(
          "__contains__",
          [](const native::OptionMarket& self, nb::handle key) {
            return self.contains(key);
          },
          nb::arg("key").none())
      .def("__len__", &native::OptionMarket::size)
      .def_prop_ro("broker", &native::OptionMarket::broker)
      .def("keys", &native::OptionMarket::keys)
      .def("values", &native::OptionMarket::values)
      .def("items", &native::OptionMarket::items)
      .def("as_dict", &native::OptionMarket::as_dict);
}

template <typename AggregatorType>
void bind_window_aggregator(
    nb::module_& m,
    const char* name) {
  nb::class_<AggregatorType>(m, name)
      .def(
          "__init__",
          [](
             AggregatorType* self,
             nb::handle rows,
             std::uint64_t interval_seconds,
             std::uint64_t offset_seconds) {
            new (self) AggregatorType(
                rows,
                interval_seconds,
                offset_seconds);
          },
          nb::arg("rows"),
          nb::arg("interval_seconds"),
          nb::arg("offset_seconds") = 0)
      .def(
          "__iter__",
          [](AggregatorType& self) -> AggregatorType& { return self.iter(); },
          nb::rv_policy::reference_internal)
      .def("__next__", &AggregatorType::next);
}

#define MASSIVE_SPEEDUP_BIND_AGG_STRING(class_, type, field) \
  class_.def_prop_ro(#field, [](const type& self) { \
    return self.cached_string(type::field##_attribute, self.field); \
  })

#define MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(class_, type, field) \
  class_.def_prop_ro(#field, [](const type& self) { \
    return self.cached_double(type::field##_attribute, self.field); \
  })

#define MASSIVE_SPEEDUP_BIND_AGG_UINT64(class_, type, field) \
  class_.def_prop_ro(#field, [](const type& self) { \
    return self.cached_uint64(type::field##_attribute, self.field); \
  })

inline void bind_aggregation_results(nb::module_& m) {
  auto stock_trade_aggregation =
      nb::class_<native::StockTradeAggregation>(m, "StockTradeAggregation");
  MASSIVE_SPEEDUP_BIND_AGG_STRING(
      stock_trade_aggregation, native::StockTradeAggregation, ticker);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_trade_aggregation, native::StockTradeAggregation, open);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_trade_aggregation, native::StockTradeAggregation, close);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_trade_aggregation, native::StockTradeAggregation, high);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_trade_aggregation, native::StockTradeAggregation, low);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_trade_aggregation, native::StockTradeAggregation, avg);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_trade_aggregation, native::StockTradeAggregation, volume_weighted_avg);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_trade_aggregation, native::StockTradeAggregation, volume);
  MASSIVE_SPEEDUP_BIND_AGG_UINT64(
      stock_trade_aggregation, native::StockTradeAggregation, window_start);
  MASSIVE_SPEEDUP_BIND_AGG_UINT64(
      stock_trade_aggregation, native::StockTradeAggregation, transactions);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_trade_aggregation, native::StockTradeAggregation, stddev);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_trade_aggregation, native::StockTradeAggregation, dollar_volume);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_trade_aggregation, native::StockTradeAggregation, avg_trade_size);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_trade_aggregation, native::StockTradeAggregation, min_trade_size);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_trade_aggregation, native::StockTradeAggregation, max_trade_size);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_trade_aggregation, native::StockTradeAggregation, price_change);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_trade_aggregation, native::StockTradeAggregation, return_bps);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_trade_aggregation, native::StockTradeAggregation, price_range);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_trade_aggregation, native::StockTradeAggregation, range_bps);
  MASSIVE_SPEEDUP_BIND_AGG_UINT64(
      stock_trade_aggregation, native::StockTradeAggregation, first_timestamp);
  MASSIVE_SPEEDUP_BIND_AGG_UINT64(
      stock_trade_aggregation, native::StockTradeAggregation, last_timestamp);
  MASSIVE_SPEEDUP_BIND_AGG_UINT64(
      stock_trade_aggregation, native::StockTradeAggregation, duration_ns);

  auto stock_quote_aggregation =
      nb::class_<native::StockQuoteAggregation>(m, "StockQuoteAggregation");
  MASSIVE_SPEEDUP_BIND_AGG_STRING(
      stock_quote_aggregation, native::StockQuoteAggregation, ticker);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, ask_open);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, ask_close);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, ask_high);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, ask_low);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, ask_avg);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, ask_volume_weighted_avg);
  MASSIVE_SPEEDUP_BIND_AGG_UINT64(
      stock_quote_aggregation, native::StockQuoteAggregation, ask_volume);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, ask_stddev);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, bid_open);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, bid_close);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, bid_high);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, bid_low);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, bid_avg);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, bid_volume_weighted_avg);
  MASSIVE_SPEEDUP_BIND_AGG_UINT64(
      stock_quote_aggregation, native::StockQuoteAggregation, bid_volume);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, bid_stddev);
  MASSIVE_SPEEDUP_BIND_AGG_UINT64(
      stock_quote_aggregation, native::StockQuoteAggregation, window_start);
  MASSIVE_SPEEDUP_BIND_AGG_UINT64(
      stock_quote_aggregation, native::StockQuoteAggregation, transactions);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, ask_change);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, ask_return_bps);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, ask_range);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, ask_range_bps);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, bid_change);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, bid_return_bps);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, bid_range);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, bid_range_bps);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, spread_open);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, spread_close);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, spread_high);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, spread_low);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, spread_avg);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, spread_stddev);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, spread_change);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, spread_return_bps);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, spread_range);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, spread_range_bps);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, mid_open);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, mid_close);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, mid_high);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, mid_low);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, mid_avg);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, mid_stddev);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, mid_change);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, mid_return_bps);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, mid_range);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, mid_range_bps);
  MASSIVE_SPEEDUP_BIND_AGG_UINT64(
      stock_quote_aggregation, native::StockQuoteAggregation, locked_count);
  MASSIVE_SPEEDUP_BIND_AGG_UINT64(
      stock_quote_aggregation, native::StockQuoteAggregation, crossed_count);
  MASSIVE_SPEEDUP_BIND_AGG_UINT64(
      stock_quote_aggregation, native::StockQuoteAggregation, zero_ask_size_count);
  MASSIVE_SPEEDUP_BIND_AGG_UINT64(
      stock_quote_aggregation, native::StockQuoteAggregation, zero_bid_size_count);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, size_imbalance_avg);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, microprice_avg);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, time_weighted_ask_avg);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, time_weighted_bid_avg);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, time_weighted_mid_avg);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      stock_quote_aggregation, native::StockQuoteAggregation, time_weighted_spread_avg);
  MASSIVE_SPEEDUP_BIND_AGG_UINT64(
      stock_quote_aggregation, native::StockQuoteAggregation, first_timestamp);
  MASSIVE_SPEEDUP_BIND_AGG_UINT64(
      stock_quote_aggregation, native::StockQuoteAggregation, last_timestamp);
  MASSIVE_SPEEDUP_BIND_AGG_UINT64(
      stock_quote_aggregation, native::StockQuoteAggregation, duration_ns);

  auto currency_quote_aggregation =
      nb::class_<native::CurrencyQuoteAggregation>(m, "CurrencyQuoteAggregation");
  MASSIVE_SPEEDUP_BIND_AGG_STRING(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, ticker);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, ask_open);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, ask_close);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, ask_high);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, ask_low);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, ask_avg);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, ask_stddev);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, bid_open);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, bid_close);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, bid_high);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, bid_low);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, bid_avg);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, bid_stddev);
  MASSIVE_SPEEDUP_BIND_AGG_UINT64(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, window_start);
  MASSIVE_SPEEDUP_BIND_AGG_UINT64(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, transactions);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, ask_change);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, ask_return_bps);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, ask_range);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, ask_range_bps);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, bid_change);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, bid_return_bps);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, bid_range);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, bid_range_bps);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, spread_open);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, spread_close);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, spread_high);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, spread_low);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, spread_avg);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, spread_stddev);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, spread_change);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, spread_return_bps);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, spread_range);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, spread_range_bps);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, mid_open);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, mid_close);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, mid_high);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, mid_low);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, mid_avg);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, mid_stddev);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, mid_change);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, mid_return_bps);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, mid_range);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, mid_range_bps);
  MASSIVE_SPEEDUP_BIND_AGG_UINT64(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, locked_count);
  MASSIVE_SPEEDUP_BIND_AGG_UINT64(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, crossed_count);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, time_weighted_ask_avg);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, time_weighted_bid_avg);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, time_weighted_mid_avg);
  MASSIVE_SPEEDUP_BIND_AGG_DOUBLE(
      currency_quote_aggregation,
      native::CurrencyQuoteAggregation,
      time_weighted_spread_avg);
  MASSIVE_SPEEDUP_BIND_AGG_UINT64(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, first_timestamp);
  MASSIVE_SPEEDUP_BIND_AGG_UINT64(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, last_timestamp);
  MASSIVE_SPEEDUP_BIND_AGG_UINT64(
      currency_quote_aggregation, native::CurrencyQuoteAggregation, duration_ns);
}

#undef MASSIVE_SPEEDUP_BIND_AGG_STRING
#undef MASSIVE_SPEEDUP_BIND_AGG_DOUBLE
#undef MASSIVE_SPEEDUP_BIND_AGG_UINT64

inline void bind_window_aggregators(nb::module_& m) {
  bind_aggregation_results(m);

  bind_window_aggregator<native::StockTradeAggregator>(
      m,
      "StockTradeAggregator");
  bind_window_aggregator<native::StockQuoteAggregator>(
      m,
      "StockQuoteAggregator");
  bind_window_aggregator<native::CurrencyQuoteAggregator>(
      m,
      "CurrencyQuoteAggregator");
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
void bind_crypto_trade_rows_iterator(nb::module_& m, const char* iterator_name) {
  bind_iterator_type<CryptoTradeRowsIterator<ParserType>>(m, iterator_name);
}

template <typename ParserType>
void bind_option_trade_rows_iterator(nb::module_& m, const char* iterator_name) {
  bind_iterator_type<OptionTradeRowsIterator<ParserType>>(m, iterator_name);
}

template <typename ParserType>
void bind_option_quote_rows_iterator(nb::module_& m, const char* iterator_name) {
  bind_iterator_type<OptionQuoteRowsIterator<ParserType>>(m, iterator_name);
}

template <typename ParserType>
void bind_quote_rows_iterator(nb::module_& m, const char* iterator_name) {
  bind_iterator_type<StockQuoteRowsIterator<ParserType>>(m, iterator_name);
}

template <typename ParserType>
void bind_futures_trade_rows_iterator(nb::module_& m, const char* iterator_name) {
  bind_iterator_type<FuturesTradeRowsIterator<ParserType>>(m, iterator_name);
}

template <typename ParserType>
void bind_futures_quote_rows_iterator(nb::module_& m, const char* iterator_name) {
  bind_iterator_type<FuturesQuoteRowsIterator<ParserType>>(m, iterator_name);
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
void bind_raw_crypto_trade_rows_iterator(nb::module_& m, const char* iterator_name) {
  bind_iterator_type<RawCryptoTradeRowsIterator<ParserType>>(m, iterator_name);
}

template <typename ParserType>
void bind_raw_option_trade_rows_iterator(nb::module_& m, const char* iterator_name) {
  bind_iterator_type<RawOptionTradeRowsIterator<ParserType>>(m, iterator_name);
}

template <typename ParserType>
void bind_raw_option_quote_rows_iterator(nb::module_& m, const char* iterator_name) {
  bind_iterator_type<RawOptionQuoteRowsIterator<ParserType>>(m, iterator_name);
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
void bind_sip_timestamp_ordering(nb::class_<RowType>& class_) {
  class_
      .def(
          "__lt__",
          [](const RowType& lhs, const RowType& rhs) {
            return lhs.sip_timestamp < rhs.sip_timestamp;
          },
          nb::is_operator())
      .def(
          "__le__",
          [](const RowType& lhs, const RowType& rhs) {
            return lhs.sip_timestamp <= rhs.sip_timestamp;
          },
          nb::is_operator())
      .def(
          "__gt__",
          [](const RowType& lhs, const RowType& rhs) {
            return lhs.sip_timestamp > rhs.sip_timestamp;
          },
          nb::is_operator())
      .def(
          "__ge__",
          [](const RowType& lhs, const RowType& rhs) {
            return lhs.sip_timestamp >= rhs.sip_timestamp;
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

inline std::string option_contract_key_from_arguments(
    const std::string& root,
    const std::string& expiration,
    const std::string& right,
    double strike) {
  return native::detail::option_contract_key(root, expiration, right, strike);
}

template <typename RowType>
void construct_option_row_from_packed(
    RowType* self,
    nb::bytes packed,
    const std::string& root,
    const std::string& expiration,
    const std::string& right,
    double strike) {
  new (self) RowType(RowType::from_packed(
      bytes_view(packed),
      option_contract_key_from_arguments(root, expiration, right, strike)));
}

template <typename RowType>
RowType option_row_from_packed(
    nb::bytes packed,
    const std::string& root,
    const std::string& expiration,
    const std::string& right,
    double strike) {
  return RowType::from_packed(
      bytes_view(packed),
      option_contract_key_from_arguments(root, expiration, right, strike));
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
          .def_prop_ro("ticker", &native::StockTrade::ticker_object)
          .def_prop_ro("conditions", &native::StockTrade::conditions_object)
          .def("updates_high_low", &native::StockTrade::updates_high_low)
          .def("updates_open_close", &native::StockTrade::updates_open_close)
          .def("updates_volume", &native::StockTrade::updates_volume)
          .def_prop_ro("correction", &native::StockTrade::correction_object)
          .def_prop_ro("exchange", &native::StockTrade::exchange_object)
          .def_prop_ro("id", &native::StockTrade::id_object)
          .def_prop_ro(
              "participant_timestamp",
              &native::StockTrade::participant_timestamp_object)
          .def_prop_ro("price", &native::StockTrade::price_object)
          .def_prop_ro(
              "sequence_number",
              &native::StockTrade::sequence_number_object)
          .def_prop_ro("sip_timestamp", &native::StockTrade::sip_timestamp_object)
          .def_prop_ro("size", &native::StockTrade::size_object)
          .def_prop_ro("decimal_size", &native::StockTrade::decimal_size_object)
          .def_prop_ro(
              "size_coefficient",
              &native::StockTrade::size_coefficient_object)
          .def_prop_ro("size_scale", &native::StockTrade::size_scale_object)
          .def_prop_ro("tape", &native::StockTrade::tape_object)
          .def_prop_ro("trf_id", &native::StockTrade::trf_id_object)
          .def_prop_ro("trf_timestamp", &native::StockTrade::trf_timestamp_object)
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
  stock_trade.attr("packed_size_offset") =
      nb::int_(native::StockTrade::packed_size_offset);
  stock_trade.attr("packed_size_scale_offset") =
      nb::int_(native::StockTrade::packed_size_scale_offset);
  bind_participant_timestamp_ordering(stock_trade);

  auto crypto_trade =
      nb::class_<native::CryptoTrade>(m, "CryptoTrade")
          .def(
              "__init__",
              [](native::CryptoTrade* self,
                 const std::vector<std::string>& fields) {
                new (self) native::CryptoTrade(
                    native::CryptoTrade::template from_fields<Specialization>(fields));
              },
              nb::arg("fields"))
          .def_prop_ro("ticker", &native::CryptoTrade::ticker_object)
          .def_prop_ro("conditions", &native::CryptoTrade::conditions_object)
          .def_prop_ro("exchange", &native::CryptoTrade::exchange_object)
          .def_prop_ro("id", &native::CryptoTrade::id_object)
          .def_prop_ro(
              "participant_timestamp",
              &native::CryptoTrade::participant_timestamp_object)
          .def_prop_ro("price", &native::CryptoTrade::price_object)
          .def_prop_ro("size", &native::CryptoTrade::size_object)
          .def(
              "__iter__",
              [](const native::CryptoTrade& self) {
                return native::detail::tuple_iterator(self.python_fields());
              })
          .def("pack", &native::CryptoTrade::packed_bytes)
          .def_static(
              "from_packed",
              &row_from_packed<native::CryptoTrade>,
              nb::arg("packed"),
              nb::arg("ticker"))
          .def_static(
              "participant_timestamp_from_packed",
              &participant_timestamp_from_packed<native::CryptoTrade>,
              nb::arg("packed"))
          .def("__hash__", &native::CryptoTrade::hash_value)
          .def("__str__", &native::CryptoTrade::repr)
          .def("__repr__", &native::CryptoTrade::repr)
          .def(
              "__eq__",
              [](const native::CryptoTrade& lhs,
                 const native::CryptoTrade& rhs) { return lhs == rhs; },
              nb::is_operator());
  crypto_trade.attr("packed_size") = nb::int_(native::CryptoTrade::packed_size);
  crypto_trade.attr("packed_participant_timestamp_offset") =
      nb::int_(native::CryptoTrade::packed_participant_timestamp_offset);
  crypto_trade.attr("packed_size_offset") =
      nb::int_(native::CryptoTrade::packed_size_offset);
  bind_participant_timestamp_ordering(crypto_trade);

  auto option_trade =
      nb::class_<native::OptionTrade>(m, "OptionTrade")
          .def(
              "__init__",
              [](native::OptionTrade* self,
                 const std::vector<std::string>& fields) {
                new (self) native::OptionTrade(
                    native::OptionTrade::template from_fields<Specialization>(fields));
              },
              nb::arg("fields"))
          .def(
              "__init__",
              &construct_option_row_from_packed<native::OptionTrade>,
              nb::arg("packed"),
              nb::arg("root"),
              nb::arg("expiration"),
              nb::arg("right"),
              nb::arg("strike"))
          .def_prop_ro("root", &native::OptionTrade::root_object)
          .def_prop_ro("expiration", &native::OptionTrade::expiration_object)
          .def_prop_ro("right", &native::OptionTrade::right_object)
          .def_prop_ro("strike", &native::OptionTrade::strike_object)
          .def_prop_ro("conditions", &native::OptionTrade::conditions_object)
          .def_prop_ro("correction", &native::OptionTrade::correction_object)
          .def_prop_ro("exchange", &native::OptionTrade::exchange_object)
          .def_prop_ro("price", &native::OptionTrade::price_object)
          .def_prop_ro("sip_timestamp", &native::OptionTrade::sip_timestamp_object)
          .def_prop_ro("size", &native::OptionTrade::size_object)
          .def(
              "__iter__",
              [](const native::OptionTrade& self) {
                return native::detail::tuple_iterator(self.python_fields());
              })
          .def("pack", &native::OptionTrade::packed_bytes)
          .def_static(
              "from_packed",
              &option_row_from_packed<native::OptionTrade>,
              nb::arg("packed"),
              nb::arg("root"),
              nb::arg("expiration"),
              nb::arg("right"),
              nb::arg("strike"))
          .def_static(
              "sip_timestamp_from_packed",
              [](nb::bytes packed) {
                const auto view = bytes_view(packed);
                native::detail::require_packed_size(
                    "OptionTrade",
                    view.size(),
                    native::OptionTrade::packed_size);
                return native::OptionTrade::sip_timestamp_at(view.data());
              },
              nb::arg("packed"))
          .def("__hash__", &native::OptionTrade::hash_value)
          .def("__str__", &native::OptionTrade::repr)
          .def("__repr__", &native::OptionTrade::repr)
          .def(
              "__eq__",
              [](const native::OptionTrade& lhs,
                 const native::OptionTrade& rhs) { return lhs == rhs; },
              nb::is_operator());
  option_trade.attr("packed_size") = nb::int_(native::OptionTrade::packed_size);
  option_trade.attr("packed_sip_timestamp_offset") =
      nb::int_(native::OptionTrade::packed_sip_timestamp_offset);
  bind_sip_timestamp_ordering(option_trade);

  auto option_quote =
      nb::class_<native::OptionQuote>(m, "OptionQuote")
          .def(
              "__init__",
              [](native::OptionQuote* self,
                 const std::vector<std::string>& fields) {
                new (self) native::OptionQuote(
                    native::OptionQuote::template from_fields<Specialization>(fields));
              },
              nb::arg("fields"))
          .def(
              "__init__",
              &construct_option_row_from_packed<native::OptionQuote>,
              nb::arg("packed"),
              nb::arg("root"),
              nb::arg("expiration"),
              nb::arg("right"),
              nb::arg("strike"))
          .def_prop_ro("root", &native::OptionQuote::root_object)
          .def_prop_ro("expiration", &native::OptionQuote::expiration_object)
          .def_prop_ro("right", &native::OptionQuote::right_object)
          .def_prop_ro("strike", &native::OptionQuote::strike_object)
          .def_prop_ro("ask_exchange", &native::OptionQuote::ask_exchange_object)
          .def_prop_ro("ask_price", &native::OptionQuote::ask_price_object)
          .def_prop_ro("ask_size", &native::OptionQuote::ask_size_object)
          .def_prop_ro("bid_exchange", &native::OptionQuote::bid_exchange_object)
          .def_prop_ro("bid_price", &native::OptionQuote::bid_price_object)
          .def_prop_ro("bid_size", &native::OptionQuote::bid_size_object)
          .def_prop_ro("sequence_number", &native::OptionQuote::sequence_number_object)
          .def_prop_ro("sip_timestamp", &native::OptionQuote::sip_timestamp_object)
          .def(
              "__iter__",
              [](const native::OptionQuote& self) {
                return native::detail::tuple_iterator(self.python_fields());
              })
          .def("pack", &native::OptionQuote::packed_bytes)
          .def_static(
              "from_packed",
              &option_row_from_packed<native::OptionQuote>,
              nb::arg("packed"),
              nb::arg("root"),
              nb::arg("expiration"),
              nb::arg("right"),
              nb::arg("strike"))
          .def_static(
              "sip_timestamp_from_packed",
              [](nb::bytes packed) {
                const auto view = bytes_view(packed);
                native::detail::require_packed_size(
                    "OptionQuote",
                    view.size(),
                    native::OptionQuote::packed_size);
                return native::OptionQuote::sip_timestamp_at(view.data());
              },
              nb::arg("packed"))
          .def("__hash__", &native::OptionQuote::hash_value)
          .def("__str__", &native::OptionQuote::repr)
          .def("__repr__", &native::OptionQuote::repr)
          .def(
              "__eq__",
              [](const native::OptionQuote& lhs,
                 const native::OptionQuote& rhs) { return lhs == rhs; },
              nb::is_operator());
  option_quote.attr("packed_size") = nb::int_(native::OptionQuote::packed_size);
  option_quote.attr("packed_sip_timestamp_offset") =
      nb::int_(native::OptionQuote::packed_sip_timestamp_offset);
  bind_sip_timestamp_ordering(option_quote);

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
          .def_prop_ro("ticker", &native::StockQuote::ticker_object)
          .def_prop_ro("ask_exchange", &native::StockQuote::ask_exchange_object)
          .def_prop_ro("ask_price", &native::StockQuote::ask_price_object)
          .def_prop_ro("ask_size", &native::StockQuote::ask_size_object)
          .def_prop_ro("bid_exchange", &native::StockQuote::bid_exchange_object)
          .def_prop_ro("bid_price", &native::StockQuote::bid_price_object)
          .def_prop_ro("bid_size", &native::StockQuote::bid_size_object)
          .def_prop_ro("conditions", &native::StockQuote::conditions_object)
          .def_prop_ro("indicators", &native::StockQuote::indicators_object)
          .def("updates_high_low", &native::StockQuote::updates_high_low)
          .def("updates_open_close", &native::StockQuote::updates_open_close)
          .def("updates_volume", &native::StockQuote::updates_volume)
          .def_prop_ro(
              "participant_timestamp",
              &native::StockQuote::participant_timestamp_object)
          .def_prop_ro(
              "sequence_number",
              &native::StockQuote::sequence_number_object)
          .def_prop_ro("sip_timestamp", &native::StockQuote::sip_timestamp_object)
          .def_prop_ro("tape", &native::StockQuote::tape_object)
          .def_prop_ro("trf_timestamp", &native::StockQuote::trf_timestamp_object)
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

  auto futures_trade =
      nb::class_<native::FuturesTrade>(m, "FuturesTrade")
          .def(
              "__init__",
              [](native::FuturesTrade* self,
                 const std::vector<std::string>& fields) {
                new (self) native::FuturesTrade(
                    native::FuturesTrade::template from_fields<Specialization>(fields));
              },
              nb::arg("fields"))
          .def(
              "__init__",
              &construct_row_from_packed_with_ticker<native::FuturesTrade>,
              nb::arg("packed"),
              nb::arg("ticker"))
          .def_prop_ro("ticker", &native::FuturesTrade::ticker_object)
          .def_prop_ro("timestamp", &native::FuturesTrade::timestamp_object)
          .def_prop_ro("sequence_number", &native::FuturesTrade::sequence_number_object)
          .def_prop_ro("report_sequence", &native::FuturesTrade::report_sequence_object)
          .def_prop_ro("price", &native::FuturesTrade::price_object)
          .def_prop_ro("size", &native::FuturesTrade::size_object)
          .def_prop_ro("correction", &native::FuturesTrade::correction_object)
          .def_prop_ro("exchange", &native::FuturesTrade::exchange_object)
          .def(
              "__iter__",
              [](const native::FuturesTrade& self) {
                return native::detail::tuple_iterator(self.python_fields());
              })
          .def("pack", &native::FuturesTrade::packed_bytes)
          .def_static(
              "from_packed",
              &row_from_packed<native::FuturesTrade>,
              nb::arg("packed"),
              nb::arg("ticker"))
          .def_static(
              "timestamp_from_packed",
              [](nb::bytes packed) {
                const auto view = bytes_view(packed);
                native::detail::require_packed_size(
                    "FuturesTrade",
                    view.size(),
                    native::FuturesTrade::packed_size);
                return native::FuturesTrade::timestamp_at(view.data());
              },
              nb::arg("packed"))
          .def("__hash__", &native::FuturesTrade::hash_value)
          .def("__str__", &native::FuturesTrade::repr)
          .def("__repr__", &native::FuturesTrade::repr)
          .def(
              "__eq__",
              [](const native::FuturesTrade& lhs,
                 const native::FuturesTrade& rhs) { return lhs == rhs; },
              nb::is_operator());
  futures_trade.attr("packed_size") = nb::int_(native::FuturesTrade::packed_size);
  futures_trade.attr("packed_timestamp_offset") =
      nb::int_(native::FuturesTrade::packed_timestamp_offset);
  futures_trade.attr("packed_size_offset") =
      nb::int_(native::FuturesTrade::packed_size_offset);

  auto futures_quote =
      nb::class_<native::FuturesQuote>(m, "FuturesQuote")
          .def(
              "__init__",
              [](native::FuturesQuote* self,
                 const std::vector<std::string>& fields) {
                new (self) native::FuturesQuote(
                    native::FuturesQuote::template from_fields<Specialization>(fields));
              },
              nb::arg("fields"))
          .def(
              "__init__",
              &construct_row_from_packed_with_ticker<native::FuturesQuote>,
              nb::arg("packed"),
              nb::arg("ticker"))
          .def_prop_ro("ticker", &native::FuturesQuote::ticker_object)
          .def_prop_ro("timestamp", &native::FuturesQuote::timestamp_object)
          .def_prop_ro("sequence_number", &native::FuturesQuote::sequence_number_object)
          .def_prop_ro("report_sequence", &native::FuturesQuote::report_sequence_object)
          .def_prop_ro("ask_timestamp", &native::FuturesQuote::ask_timestamp_object)
          .def_prop_ro("ask_price", &native::FuturesQuote::ask_price_object)
          .def_prop_ro("ask_size", &native::FuturesQuote::ask_size_object)
          .def_prop_ro("bid_timestamp", &native::FuturesQuote::bid_timestamp_object)
          .def_prop_ro("bid_price", &native::FuturesQuote::bid_price_object)
          .def_prop_ro("bid_size", &native::FuturesQuote::bid_size_object)
          .def_prop_ro("exchange", &native::FuturesQuote::exchange_object)
          .def(
              "__iter__",
              [](const native::FuturesQuote& self) {
                return native::detail::tuple_iterator(self.python_fields());
              })
          .def("pack", &native::FuturesQuote::packed_bytes)
          .def_static(
              "from_packed",
              &row_from_packed<native::FuturesQuote>,
              nb::arg("packed"),
              nb::arg("ticker"))
          .def_static(
              "timestamp_from_packed",
              [](nb::bytes packed) {
                const auto view = bytes_view(packed);
                native::detail::require_packed_size(
                    "FuturesQuote",
                    view.size(),
                    native::FuturesQuote::packed_size);
                return native::FuturesQuote::timestamp_at(view.data());
              },
              nb::arg("packed"))
          .def("__hash__", &native::FuturesQuote::hash_value)
          .def("__str__", &native::FuturesQuote::repr)
          .def("__repr__", &native::FuturesQuote::repr)
          .def(
              "__eq__",
              [](const native::FuturesQuote& lhs,
                 const native::FuturesQuote& rhs) { return lhs == rhs; },
              nb::is_operator());
  futures_quote.attr("packed_size") = nb::int_(native::FuturesQuote::packed_size);
  futures_quote.attr("packed_timestamp_offset") =
      nb::int_(native::FuturesQuote::packed_timestamp_offset);

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
          .def_prop_ro("ticker", &native::CurrencyQuote::ticker_object)
          .def_prop_ro("ask_exchange", &native::CurrencyQuote::ask_exchange_object)
          .def_prop_ro("ask_price", &native::CurrencyQuote::ask_price_object)
          .def_prop_ro("bid_exchange", &native::CurrencyQuote::bid_exchange_object)
          .def_prop_ro("bid_price", &native::CurrencyQuote::bid_price_object)
          .def_prop_ro("tickers", &native::CurrencyQuote::tickers_object)
          .def_prop_ro(
              "participant_timestamp",
              &native::CurrencyQuote::participant_timestamp_object)
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
          .def_prop_ro("ticker", &native::StockAggregate::ticker_object)
          .def_prop_ro("volume", &native::StockAggregate::volume_object)
          .def_prop_ro("open", &native::StockAggregate::open_object)
          .def_prop_ro("close", &native::StockAggregate::close_object)
          .def_prop_ro("high", &native::StockAggregate::high_object)
          .def_prop_ro("low", &native::StockAggregate::low_object)
          .def_prop_ro("window_start", &native::StockAggregate::window_start_object)
          .def_prop_ro("transactions", &native::StockAggregate::transactions_object)
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
          .def_prop_ro("ticker", &native::CurrencyAggregate::ticker_object)
          .def_prop_ro("volume", &native::CurrencyAggregate::volume_object)
          .def_prop_ro("open", &native::CurrencyAggregate::open_object)
          .def_prop_ro("close", &native::CurrencyAggregate::close_object)
          .def_prop_ro("high", &native::CurrencyAggregate::high_object)
          .def_prop_ro("low", &native::CurrencyAggregate::low_object)
          .def_prop_ro("window_start", &native::CurrencyAggregate::window_start_object)
          .def_prop_ro("transactions", &native::CurrencyAggregate::transactions_object)
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
  flatfiles.attr("CryptoTrade") = m.attr("CryptoTrade");
  flatfiles.attr("OptionTrade") = m.attr("OptionTrade");
  flatfiles.attr("OptionQuote") = m.attr("OptionQuote");
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
  static_cast<void>(raw_line_iterator_name);

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
void bind_futures_flatfile_asset(
    nb::module_& module,
    const char* name,
    const char* trade_iterator_name,
    const char* quote_iterator_name,
    const char* trade_api_name,
    const char* quote_api_name) {
  using TradeIterator = FuturesTradeRowsIterator<ImplAsset>;
  using QuoteIterator = FuturesQuoteRowsIterator<ImplAsset>;
  using TradeApi = FuturesTradeApi<ImplAsset>;
  using QuoteApi = FuturesQuoteApi<ImplAsset>;

  bind_futures_trade_rows_iterator<ImplAsset>(module, trade_iterator_name);
  bind_futures_quote_rows_iterator<ImplAsset>(module, quote_iterator_name);

  nb::class_<ImplAsset, BaseAsset> futures_class(module, name);
  futures_class
      .def(nb::init<>())
      .def_static(
          "parse_trades",
          [](const std::filesystem::path& path) {
            return TradeIterator(path);
          },
          nb::arg("path"))
      .def_static(
          "parse_quotes",
          [](const std::filesystem::path& path) {
            return QuoteIterator(path);
          },
          nb::arg("path"))
      .def_static("serialize", &serialize_static<ImplAsset>)
      .def_static("processor_name", &processor_name_static<ImplAsset>);

  nb::class_<TradeApi>(module, trade_api_name)
      .def_static(
          "parse",
          [](const std::filesystem::path& path) {
            return TradeIterator(path);
          },
          nb::arg("path"));

  nb::class_<QuoteApi>(module, quote_api_name)
      .def_static(
          "parse",
          [](const std::filesystem::path& path) {
            return QuoteIterator(path);
          },
          nb::arg("path"));

  futures_class.attr("Trade") = module.attr(trade_api_name);
  futures_class.attr("Quote") = module.attr(quote_api_name);
}

template <typename BaseAsset, typename ImplAsset>
void bind_crypto_flatfile_asset(
    nb::module_& module,
    const char* name,
    const char* trade_iterator_name,
    const char* raw_trade_iterator_name,
    const char* raw_line_iterator_name,
    const char* trade_api_name) {
  using TradeIterator = CryptoTradeRowsIterator<ImplAsset>;
  using RawTradeIterator = RawCryptoTradeRowsIterator<ImplAsset>;
  using RawLineIterator = RawLineRowsIterator<ImplAsset>;
  using TradeApi = CryptoTradeApi<ImplAsset>;

  bind_crypto_trade_rows_iterator<ImplAsset>(module, trade_iterator_name);
  bind_raw_crypto_trade_rows_iterator<ImplAsset>(module, raw_trade_iterator_name);
  static_cast<void>(raw_line_iterator_name);

  nb::class_<ImplAsset, BaseAsset> crypto_class(module, name);
  crypto_class
      .def(nb::init<>())
      .def_static(
          "parse_trades",
          [](const std::filesystem::path& path,
             bool sort_by_participant_timestamp) {
            return TradeIterator(path, sort_by_participant_timestamp);
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("sort_by_participant_timestamp") = false)
      .def_static(
          "parse_raw_trades",
          [](const std::filesystem::path& path,
             bool sort_by_participant_timestamp) {
            return RawTradeIterator(path, sort_by_participant_timestamp);
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("sort_by_participant_timestamp") = false)
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
             bool sort_by_participant_timestamp) {
            return TradeIterator(path, sort_by_participant_timestamp);
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("sort_by_participant_timestamp") = false)
      .def_static(
          "parse_raw",
          [](const std::filesystem::path& path,
             bool sort_by_participant_timestamp) {
            return RawTradeIterator(path, sort_by_participant_timestamp);
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("sort_by_participant_timestamp") = false)
      .def_static(
          "raw_lines",
          [](const std::filesystem::path& path) {
            return RawLineIterator(path);
          },
          nb::arg("path"));

  crypto_class.attr("Trade") = module.attr(trade_api_name);
}

template <typename BaseAsset, typename ImplAsset>
void bind_options_flatfile_asset(
    nb::module_& module,
    const char* name,
    const char* trade_iterator_name,
    const char* quote_iterator_name,
    const char* raw_trade_iterator_name,
    const char* raw_quote_iterator_name,
    const char* raw_line_iterator_name,
    const char* trade_api_name,
    const char* quote_api_name) {
  using TradeIterator = OptionTradeRowsIterator<ImplAsset>;
  using QuoteIterator = OptionQuoteRowsIterator<ImplAsset>;
  using RawTradeIterator = RawOptionTradeRowsIterator<ImplAsset>;
  using RawQuoteIterator = RawOptionQuoteRowsIterator<ImplAsset>;
  using RawLineIterator = RawLineRowsIterator<ImplAsset>;
  using TradeApi = OptionTradeApi<ImplAsset>;
  using QuoteApi = OptionQuoteApi<ImplAsset>;

  bind_option_trade_rows_iterator<ImplAsset>(module, trade_iterator_name);
  bind_option_quote_rows_iterator<ImplAsset>(module, quote_iterator_name);
  bind_raw_option_trade_rows_iterator<ImplAsset>(module, raw_trade_iterator_name);
  bind_raw_option_quote_rows_iterator<ImplAsset>(module, raw_quote_iterator_name);
  static_cast<void>(raw_line_iterator_name);

  nb::class_<ImplAsset, BaseAsset> options_class(module, name);
  options_class
      .def(nb::init<>())
      .def_static(
          "parse_trades",
          [](const std::filesystem::path& path, bool sort_by_sip_timestamp) {
            return TradeIterator(path, sort_by_sip_timestamp);
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("sort_by_sip_timestamp") = false)
      .def_static(
          "parse_raw_trades",
          [](const std::filesystem::path& path, bool sort_by_sip_timestamp) {
            return RawTradeIterator(path, sort_by_sip_timestamp);
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("sort_by_sip_timestamp") = false)
      .def_static(
          "parse_quotes",
          [](const std::filesystem::path& path, bool sort_by_sip_timestamp) {
            return QuoteIterator(path, sort_by_sip_timestamp);
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("sort_by_sip_timestamp") = false)
      .def_static(
          "parse_raw_quotes",
          [](const std::filesystem::path& path, bool sort_by_sip_timestamp) {
            return RawQuoteIterator(path, sort_by_sip_timestamp);
          },
          nb::arg("path"),
          nb::kw_only(),
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
          [](const std::filesystem::path& path, bool sort_by_sip_timestamp) {
            return TradeIterator(path, sort_by_sip_timestamp);
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("sort_by_sip_timestamp") = false)
      .def_static(
          "parse_raw",
          [](const std::filesystem::path& path, bool sort_by_sip_timestamp) {
            return RawTradeIterator(path, sort_by_sip_timestamp);
          },
          nb::arg("path"),
          nb::kw_only(),
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
          [](const std::filesystem::path& path, bool sort_by_sip_timestamp) {
            return QuoteIterator(path, sort_by_sip_timestamp);
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("sort_by_sip_timestamp") = false)
      .def_static(
          "parse_raw",
          [](const std::filesystem::path& path, bool sort_by_sip_timestamp) {
            return RawQuoteIterator(path, sort_by_sip_timestamp);
          },
          nb::arg("path"),
          nb::kw_only(),
          nb::arg("sort_by_sip_timestamp") = false)
      .def_static(
          "raw_lines",
          [](const std::filesystem::path& path) {
            return RawLineIterator(path);
          },
          nb::arg("path"));

  options_class.attr("Trade") = module.attr(trade_api_name);
  options_class.attr("Quote") = module.attr(quote_api_name);
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
  static_cast<void>(raw_line_iterator_name);

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
  const std::string crypto_trade_iterator_name =
      std::string(alias_prefix) + "CryptoTradeRowsIterator";
  const std::string option_trade_iterator_name =
      std::string(alias_prefix) + "OptionTradeRowsIterator";
  const std::string option_quote_iterator_name =
      std::string(alias_prefix) + "OptionQuoteRowsIterator";
  const std::string stock_quote_iterator_name =
      std::string(alias_prefix) + "StockQuoteRowsIterator";
  const std::string stock_aggregate_iterator_name =
      std::string(alias_prefix) + "StockAggregateRowsIterator";
  const std::string futures_trade_iterator_name =
      std::string(alias_prefix) + "FuturesTradeRowsIterator";
  const std::string futures_quote_iterator_name =
      std::string(alias_prefix) + "FuturesQuoteRowsIterator";
  const std::string raw_stock_trade_iterator_name =
      std::string(alias_prefix) + "RawStockTradeRowsIterator";
  const std::string raw_crypto_trade_iterator_name =
      std::string(alias_prefix) + "RawCryptoTradeRowsIterator";
  const std::string raw_option_trade_iterator_name =
      std::string(alias_prefix) + "RawOptionTradeRowsIterator";
  const std::string raw_option_quote_iterator_name =
      std::string(alias_prefix) + "RawOptionQuoteRowsIterator";
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
  const std::string crypto_trade_api_name =
      std::string(alias_prefix) + "CryptoTradeApi";
  const std::string option_trade_api_name =
      std::string(alias_prefix) + "OptionTradeApi";
  const std::string option_quote_api_name =
      std::string(alias_prefix) + "OptionQuoteApi";
  const std::string stock_quote_api_name =
      std::string(alias_prefix) + "StockQuoteApi";
  const std::string stock_aggregate_api_name =
      std::string(alias_prefix) + "StockAggregateApi";
  const std::string futures_trade_api_name =
      std::string(alias_prefix) + "FuturesTradeApi";
  const std::string futures_quote_api_name =
      std::string(alias_prefix) + "FuturesQuoteApi";
  const std::string currency_quote_api_name =
      std::string(alias_prefix) + "CurrencyQuoteApi";
  const std::string currency_aggregate_api_name =
      std::string(alias_prefix) + "CurrencyAggregateApi";
  const std::string stock_trade_database_iterator_name =
      std::string(alias_prefix) + "StockTradeDatabaseIterator";
  const std::string stock_quote_database_iterator_name =
      std::string(alias_prefix) + "StockQuoteDatabaseIterator";
  const std::string crypto_trade_database_iterator_name =
      std::string(alias_prefix) + "CryptoTradeDatabaseIterator";
  const std::string currency_quote_database_iterator_name =
      std::string(alias_prefix) + "CurrencyQuoteDatabaseIterator";
  const std::string futures_trade_database_iterator_name =
      std::string(alias_prefix) + "FuturesTradeDatabaseIterator";
  const std::string futures_quote_database_iterator_name =
      std::string(alias_prefix) + "FuturesQuoteDatabaseIterator";
  const std::string option_trade_database_iterator_name =
      std::string(alias_prefix) + "OptionTradeDatabaseIterator";
  const std::string option_quote_database_iterator_name =
      std::string(alias_prefix) + "OptionQuoteDatabaseIterator";

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
         const std::string& record_type,
         bool force) {
        return Impl<FlatFileStocksParser>::build_database_file(
            input_path,
            database_path,
            record_type,
            force);
      },
      nb::arg("input_path"),
      nb::arg("database_path"),
      nb::arg("record_type"),
      nb::arg("force") = false,
      nb::call_guard<nb::gil_scoped_release>());
  static_cast<void>(alias_prefix);

  bind_condition_enums(m);
  bind_gzip_lines<Impl<FlatFileStocksParser>>(m, gzip_iterator_name.c_str());
  bind_raw_line_rows_iterator<Impl<FlatFileStocksParser>>(
      m,
      raw_line_iterator_name.c_str());
  bind_common_bases(m);
  bind_database_record_file<native::StockTradeDatabase>(
      m,
      "StockTradeDatabase",
      stock_trade_database_iterator_name.c_str());
  bind_database_record_file<native::StockQuoteDatabase>(
      m,
      "StockQuoteDatabase",
      stock_quote_database_iterator_name.c_str());
  bind_database_record_file<native::CryptoTradeDatabase>(
      m,
      "CryptoTradeDatabase",
      crypto_trade_database_iterator_name.c_str());
  bind_database_record_file<native::CurrencyQuoteDatabase>(
      m,
      "CurrencyQuoteDatabase",
      currency_quote_database_iterator_name.c_str());
  bind_futures_database_record_file<native::FuturesTradeDatabase>(
      m,
      "FuturesTradeDatabase",
      futures_trade_database_iterator_name.c_str());
  bind_futures_database_record_file<native::FuturesQuoteDatabase>(
      m,
      "FuturesQuoteDatabase",
      futures_quote_database_iterator_name.c_str());
  bind_option_database_record_file<native::OptionTradeDatabase>(
      m,
      "OptionTradeDatabase",
      option_trade_database_iterator_name.c_str());
  bind_option_database_record_file<native::OptionQuoteDatabase>(
      m,
      "OptionQuoteDatabase",
      option_quote_database_iterator_name.c_str());
  bind_stock_trade_quote_timeline(m);
  bind_simple_market(m);

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
  bind_options_flatfile_asset<FlatFileOptionsParser, Impl<FlatFileOptionsParser>>(
      flatfiles,
      "Options",
      option_trade_iterator_name.c_str(),
      option_quote_iterator_name.c_str(),
      raw_option_trade_iterator_name.c_str(),
      raw_option_quote_iterator_name.c_str(),
      raw_line_iterator_name.c_str(),
      option_trade_api_name.c_str(),
      option_quote_api_name.c_str());
  bind_futures_flatfile_asset<FlatFileFuturesParser, Impl<FlatFileFuturesParser>>(
      flatfiles,
      "Futures",
      futures_trade_iterator_name.c_str(),
      futures_quote_iterator_name.c_str(),
      futures_trade_api_name.c_str(),
      futures_quote_api_name.c_str());
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
  bind_crypto_flatfile_asset<FlatFileCryptoParser, Impl<FlatFileCryptoParser>>(
      flatfiles,
      "Crypto",
      crypto_trade_iterator_name.c_str(),
      raw_crypto_trade_iterator_name.c_str(),
      raw_line_iterator_name.c_str(),
      crypto_trade_api_name.c_str());

  auto websocket = m.def_submodule("WebSocket", "Websocket parser classes.");
  bind_websocket_asset<WebSocketMessagesParser, Impl<WebSocketMessagesParser>>(websocket, "Messages");
  bind_websocket_asset<WebSocketStocksParser, Impl<WebSocketStocksParser>>(websocket, "Stocks");
  bind_websocket_asset<WebSocketOptionsParser, Impl<WebSocketOptionsParser>>(websocket, "Options");
  bind_websocket_asset<WebSocketFuturesParser, Impl<WebSocketFuturesParser>>(websocket, "Futures");
  bind_websocket_asset<WebSocketIndicesParser, Impl<WebSocketIndicesParser>>(websocket, "Indices");
  bind_websocket_asset<WebSocketForexParser, Impl<WebSocketForexParser>>(websocket, "Forex");
  bind_websocket_asset<WebSocketCryptoParser, Impl<WebSocketCryptoParser>>(websocket, "Crypto");

  bind_row_models<typename Impl<FlatFileStocksParser>::specialization_type>(m, flatfiles);
  bind_window_aggregators(m);

  flatfiles.attr("Stock").attr("Trade").attr("Aggregator") =
      m.attr("StockTradeAggregator");
  flatfiles.attr("Stock").attr("Quote").attr("Aggregator") =
      m.attr("StockQuoteAggregator");
  flatfiles.attr("currency").attr("Quote").attr("Aggregator") =
      m.attr("CurrencyQuoteAggregator");
}

}  // namespace massive_speedup::bindings
