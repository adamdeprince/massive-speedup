#pragma once

#include <algorithm>
#include <array>
#include <bitset>
#include <charconv>
#include <filesystem>
#include <generator>
#include <limits>
#include <memory>
#include <optional>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#if __has_include(<rapidgzip/ParallelGzipReader.hpp>) && __has_include(<filereader/Standard.hpp>)
  #include <filereader/Standard.hpp>
  #include <rapidgzip/ParallelGzipReader.hpp>
  #define MASSIVE_SPEEDUP_HAS_RAPIDGZIP_HEADERS 1
#else
  #define MASSIVE_SPEEDUP_HAS_RAPIDGZIP_HEADERS 0
#endif

#include "massive_speedup/parsers.hpp"

namespace massive_speedup::backend_generic {

namespace detail {

inline void require_field_count(
    std::string_view row_name,
    std::size_t actual,
    std::size_t expected) {
  if (actual != expected) {
    std::ostringstream message;
    message << row_name << " expected " << expected << " fields, received " << actual;
    throw std::invalid_argument(message.str());
  }
}

template <typename IntegerType>
IntegerType parse_integer(std::string_view text, std::string_view field_name) {
  if (text.empty()) {
    return 0;
  }

  using ParseType = std::conditional_t<std::is_signed_v<IntegerType>, long long, unsigned long long>;
  ParseType parsed = 0;
  const char* begin = text.data();
  const char* end = text.data() + text.size();
  const auto [ptr, error] = std::from_chars(begin, end, parsed, 10);
  if (error != std::errc{} || ptr != end) {
    std::ostringstream message;
    message << "unable to parse integer field " << field_name << ": " << text;
    throw std::invalid_argument(message.str());
  }

  if constexpr (std::is_signed_v<IntegerType>) {
    if (parsed < static_cast<ParseType>(std::numeric_limits<IntegerType>::min()) ||
        parsed > static_cast<ParseType>(std::numeric_limits<IntegerType>::max())) {
      std::ostringstream message;
      message << "integer field out of range " << field_name << ": " << text;
      throw std::out_of_range(message.str());
    }
  } else {
    if (parsed > static_cast<ParseType>(std::numeric_limits<IntegerType>::max())) {
      std::ostringstream message;
      message << "integer field out of range " << field_name << ": " << text;
      throw std::out_of_range(message.str());
    }
  }

  return static_cast<IntegerType>(parsed);
}

inline double parse_double(std::string_view text, std::string_view field_name) {
  if (text.empty()) {
    return 0.0;
  }

  double parsed = 0.0;
  const char* begin = text.data();
  const char* end = text.data() + text.size();
  const auto [ptr, error] = std::from_chars(begin, end, parsed);
  if (error != std::errc{} || ptr != end) {
    std::ostringstream message;
    message << "unable to parse floating-point field " << field_name << ": " << text;
    throw std::invalid_argument(message.str());
  }

  return parsed;
}

template <std::size_t BitCount>
std::bitset<BitCount> parse_bitset(std::string_view text, std::string_view field_name) {
  if (text.empty()) {
    return {};
  }

  try {
    std::bitset<BitCount> bits;
    std::size_t position = 0;

    while (position < text.size()) {
      const std::size_t start = position;
      const auto comma = text.find(',', start);
      const std::string_view token = comma == std::string_view::npos
          ? text.substr(start)
          : text.substr(start, comma - start);

      if (token.empty()) {
        throw std::invalid_argument("empty bit index");
      }

      const auto index = parse_integer<std::size_t>(token, field_name);
      if (index >= bits.size()) {
        throw std::out_of_range("bitset index out of range");
      }
      bits.set(index);

      if (comma == std::string_view::npos) {
        break;
      }
      position = comma + 1;
    }

    return bits;
  } catch (const std::exception& error) {
    std::ostringstream message;
    message << "unable to parse bitset field " << field_name << ": " << error.what();
    throw std::invalid_argument(message.str());
  }
}

inline std::size_t bitset_hash(const std::bitset<96>& bits) {
  return std::hash<std::string>{}(bits.to_string());
}

inline nanobind::object bit_indices_frozenset(const std::bitset<96>& bits) {
  nanobind::object result = nanobind::steal<nanobind::object>(PyFrozenSet_New(nullptr));
  if (!result.is_valid()) {
    throw nanobind::python_error();
  }

  for (std::size_t index = 0; index < bits.size(); ++index) {
    if (!bits.test(index)) {
      continue;
    }

    nanobind::object value = nanobind::steal<nanobind::object>(PyLong_FromSize_t(index));
    if (!value.is_valid() || PySet_Add(result.ptr(), value.ptr()) != 0) {
      throw nanobind::python_error();
    }
  }

  return result;
}

inline std::string bit_indices_repr(const std::bitset<96>& bits) {
  std::ostringstream out;
  bool first = true;
  for (std::size_t index = 0; index < bits.size(); ++index) {
    if (!bits.test(index)) {
      continue;
    }

    if (first) {
      out << "frozenset({";
      first = false;
    } else {
      out << ", ";
    }
    out << index;
  }

  if (first) {
    return "frozenset()";
  }

  out << "})";
  return out.str();
}

template <typename ValueType>
inline void hash_combine(std::size_t& seed, const ValueType& value) {
  seed ^= std::hash<ValueType>{}(value) + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
}

inline void hash_combine(std::size_t& seed, const std::bitset<96>& value) {
  hash_combine(seed, bitset_hash(value));
}

inline nanobind::object tuple_iterator(nanobind::handle values) {
  PyObject* iterator = PyObject_GetIter(values.ptr());
  if (iterator == nullptr) {
    throw nanobind::python_error();
  }

  return nanobind::steal<nanobind::object>(iterator);
}

inline PyObject* static_bytes_new_ref(const char* data, Py_ssize_t size) {
  PyObject* value = PyBytes_FromStringAndSize(data, size);
  if (value == nullptr) {
    throw nanobind::python_error();
  }
  return value;
}

inline PyObject* empty_bytes_new_ref() {
  static PyObject* value = static_bytes_new_ref("", 0);
  Py_INCREF(value);
  return value;
}

inline PyObject* zero_bytes_new_ref() {
  static PyObject* value = static_bytes_new_ref("0", 1);
  Py_INCREF(value);
  return value;
}

inline PyObject* raw_bytes_new_ref(std::string_view field) {
  if (field.empty()) {
    return empty_bytes_new_ref();
  }
  if (field.size() == 1 && field[0] == '0') {
    return zero_bytes_new_ref();
  }

  PyObject* value = PyBytes_FromStringAndSize(field.data(), field.size());
  if (value == nullptr) {
    throw nanobind::python_error();
  }
  return value;
}

inline std::optional<unsigned> parse_canonical_uint8_field(std::string_view field) {
  if (field.empty() || field.size() > 3) {
    return std::nullopt;
  }
  if (field.size() > 1 && field[0] == '0') {
    return std::nullopt;
  }

  unsigned value = 0;
  for (const char digit : field) {
    if (digit < '0' || digit > '9') {
      return std::nullopt;
    }
    value = value * 10U + static_cast<unsigned>(digit - '0');
  }

  if (value > 255U) {
    return std::nullopt;
  }
  return value;
}

class RawBytesInternCache {
 public:
  RawBytesInternCache() = default;

  ~RawBytesInternCache() {
    Py_XDECREF(cached_ticker_bytes_);
    for (PyObject* value : small_uint_bytes_) {
      Py_XDECREF(value);
    }
  }

  RawBytesInternCache(const RawBytesInternCache&) = delete;
  RawBytesInternCache& operator=(const RawBytesInternCache&) = delete;

  RawBytesInternCache(RawBytesInternCache&& other) noexcept
      : cached_ticker_bytes_(std::exchange(other.cached_ticker_bytes_, nullptr)),
        cached_ticker_(std::move(other.cached_ticker_)),
        small_uint_bytes_(other.small_uint_bytes_) {
    other.small_uint_bytes_.fill(nullptr);
  }

  RawBytesInternCache& operator=(RawBytesInternCache&& other) noexcept {
    if (this == &other) {
      return *this;
    }

    Py_XDECREF(cached_ticker_bytes_);
    for (PyObject* value : small_uint_bytes_) {
      Py_XDECREF(value);
    }

    cached_ticker_bytes_ = std::exchange(other.cached_ticker_bytes_, nullptr);
    cached_ticker_ = std::move(other.cached_ticker_);
    small_uint_bytes_ = other.small_uint_bytes_;
    other.small_uint_bytes_.fill(nullptr);
    return *this;
  }

  PyObject* ticker_new_ref(std::string_view ticker) {
    if (cached_ticker_bytes_ != nullptr && ticker == cached_ticker_) {
      Py_INCREF(cached_ticker_bytes_);
      return cached_ticker_bytes_;
    }

    PyObject* value = raw_bytes_new_ref(ticker);
    Py_XDECREF(cached_ticker_bytes_);
    cached_ticker_bytes_ = value;
    cached_ticker_.assign(ticker);
    Py_INCREF(cached_ticker_bytes_);
    return value;
  }

  PyObject* small_uint_new_ref(std::string_view field) {
    if (field.empty()) {
      return empty_bytes_new_ref();
    }
    if (field.size() == 1 && field[0] == '0') {
      return zero_bytes_new_ref();
    }

    const auto parsed = parse_canonical_uint8_field(field);
    if (!parsed) {
      return raw_bytes_new_ref(field);
    }

    PyObject*& cached = small_uint_bytes_[*parsed];
    if (cached == nullptr) {
      cached = PyBytes_FromStringAndSize(field.data(), field.size());
      if (cached == nullptr) {
        throw nanobind::python_error();
      }
    }

    Py_INCREF(cached);
    return cached;
  }

 private:
  PyObject* cached_ticker_bytes_ = nullptr;
  std::string cached_ticker_;
  std::array<PyObject*, 256> small_uint_bytes_{};
};

template <std::size_t FieldCount>
nanobind::tuple bytes_tuple(
    const std::array<std::string, FieldCount>& fields,
    RawBytesInternCache& intern_cache) {
  nanobind::tuple result = nanobind::steal<nanobind::tuple>(PyTuple_New(FieldCount));
  if (!result.is_valid()) {
    throw nanobind::python_error();
  }

  PyTuple_SET_ITEM(result.ptr(), 0, intern_cache.ticker_new_ref(fields[0]));

  for (std::size_t index = 1; index < FieldCount; ++index) {
    PyTuple_SET_ITEM(result.ptr(), index, raw_bytes_new_ref(fields[index]));
  }
  return result;
}

class BufferedGzipLineReader {
 public:
  explicit BufferedGzipLineReader(
      std::filesystem::path path,
      std::size_t parallelization = 0,
      std::size_t chunk_size = 1U << 20)
      : buffer_(chunk_size) {
#if MASSIVE_SPEEDUP_HAS_RAPIDGZIP_HEADERS
    if (chunk_size == 0) {
      throw std::invalid_argument("chunk_size must be greater than zero");
    }

    const auto workers = parallelization == 0
        ? static_cast<std::size_t>(std::max(1u, std::thread::hardware_concurrency()))
        : parallelization;

    auto file_reader = std::make_unique<rapidgzip::StandardFileReader>(path.string());
    reader_ = std::make_unique<rapidgzip::ParallelGzipReader<rapidgzip::ChunkData>>(
        std::move(file_reader),
        workers,
        chunk_size);
#else
    static_cast<void>(path);
    static_cast<void>(parallelization);
    static_cast<void>(chunk_size);
    throw std::runtime_error(
        "rapidgzip headers are not available in the current build tree; "
        "check out the rapidgzip/librapidarchive sources before using gzip_lines");
#endif
  }

  template <typename Specialization>
  bool next_line(std::string_view& line) {
    return Specialization::next_line(*this, line);
  }

  std::string_view line_view(std::size_t start, std::size_t end) const {
#if MASSIVE_SPEEDUP_HAS_RAPIDGZIP_HEADERS
    std::size_t length = end - start;
    if (length != 0 && pending_[start + length - 1] == '\r') {
      --length;
    }

    return std::string_view(pending_.data() + start, length);
#else
    static_cast<void>(start);
    static_cast<void>(end);
    throw std::runtime_error(
        "rapidgzip headers are not available in the current build tree; "
        "check out the rapidgzip/librapidarchive sources before using gzip_lines");
#endif
  }

  void release_consumed_prefix() {
#if MASSIVE_SPEEDUP_HAS_RAPIDGZIP_HEADERS
    if (line_start_ == 0) {
      return;
    }

    if (line_start_ >= pending_.size()) {
      pending_.clear();
      line_start_ = 0;
      search_offset_ = 0;
      return;
    }

    pending_.erase(0, line_start_);
    search_offset_ -= line_start_;
    line_start_ = 0;
#endif
  }

  bool read_more() {
#if MASSIVE_SPEEDUP_HAS_RAPIDGZIP_HEADERS
    release_consumed_prefix();

    const auto bytes_read = reader_->read(-1, buffer_.data(), buffer_.size());
    if (bytes_read == 0) {
      return false;
    }

    pending_.append(buffer_.data(), bytes_read);
    return true;
#else
    throw std::runtime_error(
        "rapidgzip headers are not available in the current build tree; "
        "check out the rapidgzip/librapidarchive sources before using gzip_lines");
#endif
  }

  void clear_consumed_buffer() {
    pending_.clear();
    line_start_ = 0;
    search_offset_ = 0;
  }

  std::vector<char> buffer_;
  std::string pending_;
  std::size_t line_start_ = 0;
  std::size_t search_offset_ = 0;

 private:
#if MASSIVE_SPEEDUP_HAS_RAPIDGZIP_HEADERS
  std::unique_ptr<rapidgzip::ParallelGzipReader<rapidgzip::ChunkData>> reader_;
#endif
};

class CsvLineCursor {
 public:
  explicit CsvLineCursor(std::string_view line)
      : line_(line) {}

  template <typename Specialization, bool ExpectMore>
  std::string_view next_field(std::string& scratch) {
    if constexpr (!ExpectMore) {
      if (cursor_ == line_.size()) {
        return {};
      }
    }

    if (cursor_ > line_.size()) {
      throw std::invalid_argument("unexpected end of CSV row");
    }

    if (cursor_ < line_.size() && line_[cursor_] == '"') {
      return Specialization::template parse_quoted_field<ExpectMore>(
          line_,
          cursor_,
          scratch);
    }

    return Specialization::template parse_unquoted_field<ExpectMore>(line_, cursor_);
  }

  void finish() const {
    if (cursor_ != line_.size()) {
      throw std::invalid_argument("unexpected trailing data in CSV row");
    }
  }

  template <bool ExpectMore>
  static std::string_view scalar_parse_unquoted_field(
      std::string_view line,
      std::size_t& cursor) {
    if constexpr (ExpectMore) {
      if (cursor == line.size()) {
        throw std::invalid_argument("CSV row ended before expected delimiter");
      }

      if (line[cursor] == ',') {
        ++cursor;
        return {};
      }

      const auto comma = line.find(',', cursor);
      if (comma == std::string_view::npos) {
        throw std::invalid_argument("CSV row ended before expected delimiter");
      }

      const std::string_view result = line.substr(cursor, comma - cursor);
      cursor = comma + 1;
      return result;
    } else {
      const std::string_view result = line.substr(cursor);
      cursor = line.size();
      return result;
    }
  }

  template <bool ExpectMore>
  static std::string_view scalar_parse_quoted_field(
      std::string_view line,
      std::size_t& cursor,
      std::string& scratch) {
    ++cursor;

    std::size_t segment_start = cursor;
    bool uses_scratch = false;
    scratch.clear();

    while (cursor < line.size()) {
      if (line[cursor] != '"') {
        ++cursor;
        continue;
      }

      if (cursor + 1 < line.size() && line[cursor + 1] == '"') {
        scratch.append(line.data() + segment_start, cursor - segment_start);
        scratch.push_back('"');
        cursor += 2;
        segment_start = cursor;
        uses_scratch = true;
        continue;
      }

      break;
    }

    if (cursor >= line.size()) {
      throw std::invalid_argument("unterminated quoted CSV field");
    }

    std::string_view result;
    if (uses_scratch) {
      scratch.append(line.data() + segment_start, cursor - segment_start);
      result = scratch;
    } else {
      result = line.substr(segment_start, cursor - segment_start);
    }

    ++cursor;

    if constexpr (ExpectMore) {
      if (cursor >= line.size() || line[cursor] != ',') {
        throw std::invalid_argument("CSV row ended before expected delimiter");
      }
      ++cursor;
    } else if (cursor != line.size()) {
      throw std::invalid_argument("unexpected trailing data in CSV row");
    }

    return result;
  }

 private:
  std::string_view line_;
  std::size_t cursor_ = 0;
};

}  // namespace detail

struct StockTrade {
  std::string ticker;
  std::bitset<96> conditions;
  double price = 0.0;
  std::uint64_t id = 0;
  std::uint64_t participant_timestamp = 0;
  std::uint64_t sequence_number = 0;
  std::uint64_t sip_timestamp = 0;
  std::uint64_t trf_timestamp = 0;
  std::int32_t correction = 0;
  std::int32_t size = 0;
  std::uint16_t tape = 0;
  std::uint16_t trf_id = 0;
  std::uint8_t exchange = 0;

  template <typename Specialization>
  static StockTrade from_fields(const std::vector<std::string>& fields) {
    StockTrade result;
    detail::require_field_count("StockTrade", fields.size(), 13);
    result.ticker = fields[0];
    result.conditions = Specialization::template parse_bitset<96>(fields[1], "conditions");
    result.correction =
        Specialization::template parse_integer<std::int32_t>(fields[2], "correction");
    result.exchange =
        Specialization::template parse_integer<std::uint8_t>(fields[3], "exchange");
    result.id = Specialization::template parse_integer<std::uint64_t>(fields[4], "id");
    result.participant_timestamp = Specialization::template parse_integer<std::uint64_t>(
        fields[5],
        "participant_timestamp");
    result.price = Specialization::parse_double(fields[6], "price");
    result.sequence_number =
        Specialization::template parse_integer<std::uint64_t>(fields[7], "sequence_number");
    result.sip_timestamp =
        Specialization::template parse_integer<std::uint64_t>(fields[8], "sip_timestamp");
    result.size = Specialization::template parse_integer<std::int32_t>(fields[9], "size");
    result.tape = Specialization::template parse_integer<std::uint16_t>(fields[10], "tape");
    result.trf_id =
        Specialization::template parse_integer<std::uint16_t>(fields[11], "trf_id");
    result.trf_timestamp =
        Specialization::template parse_integer<std::uint64_t>(fields[12], "trf_timestamp");
    return result;
  }

  bool operator==(const StockTrade&) const = default;

  nanobind::object conditions_object() const {
    return detail::bit_indices_frozenset(conditions);
  }

  nanobind::list python_fields() const {
    nanobind::list values;
    values.append(nanobind::cast(ticker));
    values.append(conditions_object());
    values.append(nanobind::cast(correction));
    values.append(nanobind::cast(exchange));
    values.append(nanobind::cast(id));
    values.append(nanobind::cast(participant_timestamp));
    values.append(nanobind::cast(price));
    values.append(nanobind::cast(sequence_number));
    values.append(nanobind::cast(sip_timestamp));
    values.append(nanobind::cast(size));
    values.append(nanobind::cast(tape));
    values.append(nanobind::cast(trf_id));
    values.append(nanobind::cast(trf_timestamp));
    return values;
  }

  std::size_t hash_value() const {
    std::size_t seed = 0;
    detail::hash_combine(seed, ticker);
    detail::hash_combine(seed, conditions);
    detail::hash_combine(seed, correction);
    detail::hash_combine(seed, exchange);
    detail::hash_combine(seed, id);
    detail::hash_combine(seed, participant_timestamp);
    detail::hash_combine(seed, price);
    detail::hash_combine(seed, sequence_number);
    detail::hash_combine(seed, sip_timestamp);
    detail::hash_combine(seed, size);
    detail::hash_combine(seed, tape);
    detail::hash_combine(seed, trf_id);
    detail::hash_combine(seed, trf_timestamp);
    return seed;
  }

  std::string repr() const {
    std::ostringstream out;
    out << "StockTrade("
        << "ticker='" << ticker << "', "
        << "conditions=" << detail::bit_indices_repr(conditions) << ", "
        << "correction=" << correction << ", "
        << "exchange=" << static_cast<unsigned>(exchange) << ", "
        << "id=" << id << ", "
        << "participant_timestamp=" << participant_timestamp << ", "
        << "price=" << price << ", "
        << "sequence_number=" << sequence_number << ", "
        << "sip_timestamp=" << sip_timestamp << ", "
        << "size=" << size << ", "
        << "tape=" << tape << ", "
        << "trf_id=" << trf_id << ", "
        << "trf_timestamp=" << trf_timestamp << ")";
    return out.str();
  }
};

struct StockQuote {
  std::string ticker;
  std::bitset<96> conditions;
  std::bitset<96> indicators;
  double ask_price = 0.0;
  double bid_price = 0.0;
  std::uint64_t participant_timestamp = 0;
  std::uint64_t sequence_number = 0;
  std::uint64_t sip_timestamp = 0;
  std::uint64_t trf_timestamp = 0;
  std::uint32_t ask_size = 0;
  std::uint32_t bid_size = 0;
  std::uint8_t ask_exchange = 0;
  std::uint8_t bid_exchange = 0;
  std::uint8_t tape = 0;

  template <typename Specialization>
  static StockQuote from_fields(const std::vector<std::string>& fields) {
    StockQuote result;
    detail::require_field_count("StockQuote", fields.size(), 14);
    result.ticker = fields[0];
    result.ask_exchange =
        Specialization::template parse_integer<std::uint8_t>(fields[1], "ask_exchange");
    result.ask_price = Specialization::parse_double(fields[2], "ask_price");
    result.ask_size =
        Specialization::template parse_integer<std::uint32_t>(fields[3], "ask_size");
    result.bid_exchange =
        Specialization::template parse_integer<std::uint8_t>(fields[4], "bid_exchange");
    result.bid_price = Specialization::parse_double(fields[5], "bid_price");
    result.bid_size =
        Specialization::template parse_integer<std::uint32_t>(fields[6], "bid_size");
    result.conditions =
        Specialization::template parse_bitset<96>(fields[7], "conditions");
    result.indicators =
        Specialization::template parse_bitset<96>(fields[8], "indicators");
    result.participant_timestamp = Specialization::template parse_integer<std::uint64_t>(
        fields[9],
        "participant_timestamp");
    result.sequence_number =
        Specialization::template parse_integer<std::uint64_t>(fields[10], "sequence_number");
    result.sip_timestamp =
        Specialization::template parse_integer<std::uint64_t>(fields[11], "sip_timestamp");
    result.tape = Specialization::template parse_integer<std::uint8_t>(fields[12], "tape");
    result.trf_timestamp =
        Specialization::template parse_integer<std::uint64_t>(fields[13], "trf_timestamp");
    return result;
  }

  bool operator==(const StockQuote&) const = default;

  nanobind::object conditions_object() const {
    return detail::bit_indices_frozenset(conditions);
  }

  nanobind::object indicators_object() const {
    return detail::bit_indices_frozenset(indicators);
  }

  nanobind::list python_fields() const {
    nanobind::list values;
    values.append(nanobind::cast(ticker));
    values.append(nanobind::cast(ask_exchange));
    values.append(nanobind::cast(ask_price));
    values.append(nanobind::cast(ask_size));
    values.append(nanobind::cast(bid_exchange));
    values.append(nanobind::cast(bid_price));
    values.append(nanobind::cast(bid_size));
    values.append(conditions_object());
    values.append(indicators_object());
    values.append(nanobind::cast(participant_timestamp));
    values.append(nanobind::cast(sequence_number));
    values.append(nanobind::cast(sip_timestamp));
    values.append(nanobind::cast(tape));
    values.append(nanobind::cast(trf_timestamp));
    return values;
  }

  std::size_t hash_value() const {
    std::size_t seed = 0;
    detail::hash_combine(seed, ticker);
    detail::hash_combine(seed, ask_exchange);
    detail::hash_combine(seed, ask_price);
    detail::hash_combine(seed, ask_size);
    detail::hash_combine(seed, bid_exchange);
    detail::hash_combine(seed, bid_price);
    detail::hash_combine(seed, bid_size);
    detail::hash_combine(seed, conditions);
    detail::hash_combine(seed, indicators);
    detail::hash_combine(seed, participant_timestamp);
    detail::hash_combine(seed, sequence_number);
    detail::hash_combine(seed, sip_timestamp);
    detail::hash_combine(seed, tape);
    detail::hash_combine(seed, trf_timestamp);
    return seed;
  }

  std::string repr() const {
    std::ostringstream out;
    out << "StockQuote("
        << "ticker='" << ticker << "', "
        << "ask_exchange=" << static_cast<unsigned>(ask_exchange) << ", "
        << "ask_price=" << ask_price << ", "
        << "ask_size=" << ask_size << ", "
        << "bid_exchange=" << static_cast<unsigned>(bid_exchange) << ", "
        << "bid_price=" << bid_price << ", "
        << "bid_size=" << bid_size << ", "
        << "conditions=" << detail::bit_indices_repr(conditions) << ", "
        << "indicators=" << detail::bit_indices_repr(indicators) << ", "
        << "participant_timestamp=" << participant_timestamp << ", "
        << "sequence_number=" << sequence_number << ", "
        << "sip_timestamp=" << sip_timestamp << ", "
        << "tape=" << static_cast<unsigned>(tape) << ", "
        << "trf_timestamp=" << trf_timestamp << ")";
    return out.str();
  }
};

struct GenericSpecialization {
  static inline bool next_line(
      detail::BufferedGzipLineReader& reader,
      std::string_view& line) {
    if (reader.line_start_ >= reader.pending_.size()) {
      reader.clear_consumed_buffer();
    }

    while (true) {
      const auto newline = reader.pending_.find('\n', reader.search_offset_);
      if (newline != std::string::npos) {
        line = reader.line_view(reader.line_start_, newline);
        reader.line_start_ = newline + 1;
        reader.search_offset_ = reader.line_start_;
        return true;
      }

      reader.search_offset_ = reader.pending_.size();
      if (!reader.read_more()) {
        break;
      }
    }

    if (reader.line_start_ < reader.pending_.size()) {
      line = reader.line_view(reader.line_start_, reader.pending_.size());
      reader.line_start_ = reader.pending_.size();
      reader.search_offset_ = reader.line_start_;
      return true;
    }

    line = {};
    return false;
  }

  template <bool ExpectMore>
  static inline std::string_view parse_unquoted_field(
      std::string_view line,
      std::size_t& cursor) {
    return detail::CsvLineCursor::template scalar_parse_unquoted_field<ExpectMore>(
        line,
        cursor);
  }

  template <bool ExpectMore>
  static inline std::string_view parse_quoted_field(
      std::string_view line,
      std::size_t& cursor,
      std::string& scratch) {
    return detail::CsvLineCursor::template scalar_parse_quoted_field<ExpectMore>(
        line,
        cursor,
        scratch);
  }

  template <typename IntegerType>
  static inline IntegerType parse_integer(
      std::string_view text,
      std::string_view field_name) {
    return detail::parse_integer<IntegerType>(text, field_name);
  }

  static inline double parse_double(std::string_view text, std::string_view field_name) {
    return detail::parse_double(text, field_name);
  }

  template <std::size_t BitCount>
  static inline std::bitset<BitCount> parse_bitset(
      std::string_view text,
      std::string_view field_name) {
    return detail::parse_bitset<BitCount>(text, field_name);
  }

  static inline void split_on_commas(
      std::string_view payload,
      std::vector<std::string>& output) {
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
  }

  static inline void split_csv_fields(
      std::string_view line,
      std::vector<std::string>& output) {
    if (output.empty()) {
      output.resize(8);
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

template <typename Base, typename Specialization = GenericSpecialization>
class Implementation : public Base {
 public:
  using Base::Base;
  using specialization_type = Specialization;
  using GzipLineGenerator = std::generator<std::string>;
  using GzipLineIteratorType = decltype(std::declval<GzipLineGenerator&>().begin());
  using RawStockTrade = std::array<std::string, 13>;
  using RawStockQuote = std::array<std::string, 14>;

  class GzipLinesIterator {
   public:
    explicit GzipLinesIterator(
        const std::filesystem::path& path,
        std::size_t parallelization = 0,
        std::size_t chunk_size = 1U << 20)
        : generator_(Implementation::read_gzip_lines(path, parallelization, chunk_size)) {}

    GzipLinesIterator& iter() { return *this; }

    nanobind::bytes next() {
      if (exhausted_) {
        throw nanobind::stop_iteration();
      }

      if (!iterator_) {
        iterator_.emplace(generator_.begin());
      }

      if (*iterator_ == std::default_sentinel) {
        exhausted_ = true;
        throw nanobind::stop_iteration();
      }

      const std::string& line = **iterator_;
      nanobind::bytes result(line.data(), line.size());
      ++(*iterator_);
      return result;
    }

   private:
    GzipLineGenerator generator_;
    std::optional<GzipLineIteratorType> iterator_;
    bool exhausted_ = false;
  };

  class StockTradeStreamState {
   public:
    explicit StockTradeStreamState(const std::filesystem::path& path)
        : reader_(path) {}

    bool next_row(StockTrade& row) {
      std::string_view line;

      while (reader_.template next_line<Specialization>(line)) {
        if (is_first_line_) {
          is_first_line_ = false;
          continue;
        }

        if (line.empty()) {
          continue;
        }

        row = Implementation::parse_trade_row(line);
        return true;
      }

      return false;
    }

   private:
    detail::BufferedGzipLineReader reader_;
    bool is_first_line_ = true;
  };

  class StockQuoteStreamState {
   public:
    explicit StockQuoteStreamState(const std::filesystem::path& path)
        : reader_(path) {}

    bool next_row(StockQuote& row) {
      std::string_view line;

      while (reader_.template next_line<Specialization>(line)) {
        if (is_first_line_) {
          is_first_line_ = false;
          continue;
        }

        if (line.empty()) {
          continue;
        }

        row = Implementation::parse_quote_row(line);
        return true;
      }

      return false;
    }

   private:
    detail::BufferedGzipLineReader reader_;
    bool is_first_line_ = true;
  };

  class RawStockTradeStreamState {
   public:
    explicit RawStockTradeStreamState(const std::filesystem::path& path)
        : reader_(path) {}

    RawStockTradeStreamState(const RawStockTradeStreamState&) = delete;
    RawStockTradeStreamState& operator=(const RawStockTradeStreamState&) = delete;

    RawStockTradeStreamState(RawStockTradeStreamState&& other) noexcept
        : reader_(std::move(other.reader_)),
          is_first_line_(other.is_first_line_),
          intern_cache_(std::move(other.intern_cache_)) {}

    RawStockTradeStreamState& operator=(RawStockTradeStreamState&& other) noexcept {
      if (this == &other) {
        return *this;
      }

      reader_ = std::move(other.reader_);
      is_first_line_ = other.is_first_line_;
      intern_cache_ = std::move(other.intern_cache_);
      return *this;
    }

    bool next_row(RawStockTrade& row) {
      std::string_view line;

      while (reader_.template next_line<Specialization>(line)) {
        if (is_first_line_) {
          is_first_line_ = false;
          continue;
        }

        if (line.empty()) {
          continue;
        }

        row = Implementation::parse_raw_trade_row(line);
        return true;
      }

      return false;
    }

    bool next_tuple(nanobind::tuple& row) {
      std::string_view line;

      while (reader_.template next_line<Specialization>(line)) {
        if (is_first_line_) {
          is_first_line_ = false;
          continue;
        }

        if (line.empty()) {
          continue;
        }

        row = Implementation::parse_raw_trade_tuple(
            line,
            intern_cache_);
        return true;
      }

      return false;
    }

   private:
    detail::BufferedGzipLineReader reader_;
    bool is_first_line_ = true;
    detail::RawBytesInternCache intern_cache_;
  };

  class RawStockQuoteStreamState {
   public:
    explicit RawStockQuoteStreamState(const std::filesystem::path& path)
        : reader_(path) {}

    RawStockQuoteStreamState(const RawStockQuoteStreamState&) = delete;
    RawStockQuoteStreamState& operator=(const RawStockQuoteStreamState&) = delete;

    RawStockQuoteStreamState(RawStockQuoteStreamState&& other) noexcept
        : reader_(std::move(other.reader_)),
          is_first_line_(other.is_first_line_),
          intern_cache_(std::move(other.intern_cache_)) {}

    RawStockQuoteStreamState& operator=(RawStockQuoteStreamState&& other) noexcept {
      if (this == &other) {
        return *this;
      }

      reader_ = std::move(other.reader_);
      is_first_line_ = other.is_first_line_;
      intern_cache_ = std::move(other.intern_cache_);
      return *this;
    }

    bool next_row(RawStockQuote& row) {
      std::string_view line;

      while (reader_.template next_line<Specialization>(line)) {
        if (is_first_line_) {
          is_first_line_ = false;
          continue;
        }

        if (line.empty()) {
          continue;
        }

        row = Implementation::parse_raw_quote_row(line);
        return true;
      }

      return false;
    }

    bool next_tuple(nanobind::tuple& row) {
      std::string_view line;

      while (reader_.template next_line<Specialization>(line)) {
        if (is_first_line_) {
          is_first_line_ = false;
          continue;
        }

        if (line.empty()) {
          continue;
        }

        row = Implementation::parse_raw_quote_tuple(
            line,
            intern_cache_);
        return true;
      }

      return false;
    }

   private:
    detail::BufferedGzipLineReader reader_;
    bool is_first_line_ = true;
    detail::RawBytesInternCache intern_cache_;
  };

  class RawLineRowsIterator {
   public:
    explicit RawLineRowsIterator(const std::filesystem::path& path)
        : reader_(path) {}

    RawLineRowsIterator& iter() { return *this; }

    nanobind::bytes next() {
      std::string_view line;

      while (reader_.template next_line<Specialization>(line)) {
        if (is_first_line_) {
          is_first_line_ = false;
          continue;
        }

        if (line.empty()) {
          continue;
        }

        return nanobind::bytes(line.data(), line.size());
      }

      throw nanobind::stop_iteration();
    }

   private:
    detail::BufferedGzipLineReader reader_;
    bool is_first_line_ = true;
  };

  class StockTradeRowsIterator {
   public:
    explicit StockTradeRowsIterator(
        const std::filesystem::path& path,
        bool sort_by_participant_timestamp = false,
        bool sort_by_sip_timestamp = false) {
      validate_sort_flags(sort_by_participant_timestamp, sort_by_sip_timestamp);

      if (sort_by_participant_timestamp || sort_by_sip_timestamp) {
        rows_ = collect_rows<StockTrade>(
            path,
            sort_by_participant_timestamp,
            sort_by_sip_timestamp,
            &Implementation::parse_trade_row);
      } else {
        stream_state_.emplace(path);
      }
    }

    StockTradeRowsIterator& iter() { return *this; }

    StockTrade next() {
      return Implementation::template next_parsed_row<StockTrade, StockTradeStreamState>(
          stream_state_,
          rows_,
          row_index_);
    }

   private:
    std::optional<StockTradeStreamState> stream_state_;
    std::vector<StockTrade> rows_;
    std::size_t row_index_ = 0;
  };

  class StockQuoteRowsIterator {
   public:
    explicit StockQuoteRowsIterator(
        const std::filesystem::path& path,
        bool sort_by_participant_timestamp = false,
        bool sort_by_sip_timestamp = false) {
      validate_sort_flags(sort_by_participant_timestamp, sort_by_sip_timestamp);

      if (sort_by_participant_timestamp || sort_by_sip_timestamp) {
        rows_ = collect_rows<StockQuote>(
            path,
            sort_by_participant_timestamp,
            sort_by_sip_timestamp,
            &Implementation::parse_quote_row);
      } else {
        stream_state_.emplace(path);
      }
    }

    StockQuoteRowsIterator& iter() { return *this; }

    StockQuote next() {
      return Implementation::template next_parsed_row<StockQuote, StockQuoteStreamState>(
          stream_state_,
          rows_,
          row_index_);
    }

   private:
    std::optional<StockQuoteStreamState> stream_state_;
    std::vector<StockQuote> rows_;
    std::size_t row_index_ = 0;
  };

  class RawStockTradeRowsIterator {
   public:
    explicit RawStockTradeRowsIterator(
        const std::filesystem::path& path,
        bool sort_by_participant_timestamp = false,
        bool sort_by_sip_timestamp = false) {
      validate_sort_flags(sort_by_participant_timestamp, sort_by_sip_timestamp);

      if (sort_by_participant_timestamp || sort_by_sip_timestamp) {
        rows_ = collect_rows<RawStockTrade>(
            path,
            sort_by_participant_timestamp,
            sort_by_sip_timestamp,
            &Implementation::parse_raw_trade_row);
      } else {
        stream_state_.emplace(path);
      }
    }

    RawStockTradeRowsIterator(const RawStockTradeRowsIterator&) = delete;
    RawStockTradeRowsIterator& operator=(const RawStockTradeRowsIterator&) = delete;

    RawStockTradeRowsIterator(RawStockTradeRowsIterator&& other) noexcept
        : stream_state_(std::move(other.stream_state_)),
          rows_(std::move(other.rows_)),
          row_index_(other.row_index_),
          intern_cache_(std::move(other.intern_cache_)) {}

    RawStockTradeRowsIterator& operator=(RawStockTradeRowsIterator&& other) noexcept {
      if (this == &other) {
        return *this;
      }

      stream_state_ = std::move(other.stream_state_);
      rows_ = std::move(other.rows_);
      row_index_ = other.row_index_;
      intern_cache_ = std::move(other.intern_cache_);
      return *this;
    }

    RawStockTradeRowsIterator& iter() { return *this; }

    nanobind::tuple next() {
      if (stream_state_) {
        nanobind::tuple row;
        if (stream_state_->next_tuple(row)) {
          return row;
        }

        stream_state_.reset();
        throw nanobind::stop_iteration();
      }

      return Implementation::raw_trade_array_to_tuple(rows_next(), intern_cache_);
    }

   private:
    RawStockTrade rows_next() {
      if (row_index_ >= rows_.size()) {
        throw nanobind::stop_iteration();
      }
      return std::move(rows_[row_index_++]);
    }

    std::optional<RawStockTradeStreamState> stream_state_;
    std::vector<RawStockTrade> rows_;
    std::size_t row_index_ = 0;
    detail::RawBytesInternCache intern_cache_;
  };

  class RawStockQuoteRowsIterator {
   public:
    explicit RawStockQuoteRowsIterator(
        const std::filesystem::path& path,
        bool sort_by_participant_timestamp = false,
        bool sort_by_sip_timestamp = false) {
      validate_sort_flags(sort_by_participant_timestamp, sort_by_sip_timestamp);

      if (sort_by_participant_timestamp || sort_by_sip_timestamp) {
        rows_ = collect_rows<RawStockQuote>(
            path,
            sort_by_participant_timestamp,
            sort_by_sip_timestamp,
            &Implementation::parse_raw_quote_row);
      } else {
        stream_state_.emplace(path);
      }
    }

    RawStockQuoteRowsIterator(const RawStockQuoteRowsIterator&) = delete;
    RawStockQuoteRowsIterator& operator=(const RawStockQuoteRowsIterator&) = delete;

    RawStockQuoteRowsIterator(RawStockQuoteRowsIterator&& other) noexcept
        : stream_state_(std::move(other.stream_state_)),
          rows_(std::move(other.rows_)),
          row_index_(other.row_index_),
          intern_cache_(std::move(other.intern_cache_)) {}

    RawStockQuoteRowsIterator& operator=(RawStockQuoteRowsIterator&& other) noexcept {
      if (this == &other) {
        return *this;
      }

      stream_state_ = std::move(other.stream_state_);
      rows_ = std::move(other.rows_);
      row_index_ = other.row_index_;
      intern_cache_ = std::move(other.intern_cache_);
      return *this;
    }

    RawStockQuoteRowsIterator& iter() { return *this; }

    nanobind::tuple next() {
      if (stream_state_) {
        nanobind::tuple row;
        if (stream_state_->next_tuple(row)) {
          return row;
        }

        stream_state_.reset();
        throw nanobind::stop_iteration();
      }

      return Implementation::raw_quote_array_to_tuple(rows_next(), intern_cache_);
    }

   private:
    RawStockQuote rows_next() {
      if (row_index_ >= rows_.size()) {
        throw nanobind::stop_iteration();
      }
      return std::move(rows_[row_index_++]);
    }

    std::optional<RawStockQuoteStreamState> stream_state_;
    std::vector<RawStockQuote> rows_;
    std::size_t row_index_ = 0;
    detail::RawBytesInternCache intern_cache_;
  };

  static GzipLineGenerator read_gzip_lines(
      std::filesystem::path path,
      std::size_t parallelization = 0,
      std::size_t chunk_size = 1U << 20) {
    detail::BufferedGzipLineReader reader(std::move(path), parallelization, chunk_size);
    std::string_view line;

    while (reader.template next_line<Specialization>(line)) {
      co_yield std::string(line);
    }
  }

  std::vector<StockTrade> parse_trade_rows(
      const std::filesystem::path& path,
      bool sort_by_participant_timestamp = false,
      bool sort_by_sip_timestamp = false) const {
    return collect_rows<StockTrade>(
        path,
        sort_by_participant_timestamp,
        sort_by_sip_timestamp,
        &Implementation::parse_trade_row);
  }

  std::vector<StockQuote> parse_quote_rows(
      const std::filesystem::path& path,
      bool sort_by_participant_timestamp = false,
      bool sort_by_sip_timestamp = false) const {
    return collect_rows<StockQuote>(
        path,
        sort_by_participant_timestamp,
        sort_by_sip_timestamp,
        &Implementation::parse_quote_row);
  }

  Summary parse_message(nb::handle payload) const {
    const std::string materialized = payload_to_string(payload);
    Summary summary = this->build_summary(
        materialized,
        "parse_message",
        "json",
        &Specialization::split_on_commas);
    summary.emplace(
        "message_frames",
        nanobind::int_(
            count_substring(materialized, "},{") + count_substring(materialized, "}{") +
            (materialized.empty() ? 0 : 1)));
    return summary;
  }

  static inline void split_on_commas(
      std::string_view payload,
      std::vector<std::string>& output) {
    Specialization::split_on_commas(payload, output);
  }

 private:
  template <typename RowType>
  using ParseRowFn = RowType (*)(std::string_view);

  static StockTrade parse_trade_row(std::string_view line) {
    detail::CsvLineCursor cursor(line);
    std::string scratch;
    StockTrade result;

    result.ticker.assign(cursor.template next_field<Specialization, true>(scratch));
    result.conditions =
        Specialization::template parse_bitset<96>(
            cursor.template next_field<Specialization, true>(scratch),
            "conditions");
    result.correction =
        Specialization::template parse_integer<std::int32_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "correction");
    result.exchange =
        Specialization::template parse_integer<std::uint8_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "exchange");
    result.id =
        Specialization::template parse_integer<std::uint64_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "id");
    result.participant_timestamp =
        Specialization::template parse_integer<std::uint64_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "participant_timestamp");
    result.price = Specialization::parse_double(
        cursor.template next_field<Specialization, true>(scratch),
        "price");
    result.sequence_number =
        Specialization::template parse_integer<std::uint64_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "sequence_number");
    result.sip_timestamp =
        Specialization::template parse_integer<std::uint64_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "sip_timestamp");
    result.size =
        Specialization::template parse_integer<std::int32_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "size");
    result.tape =
        Specialization::template parse_integer<std::uint16_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "tape");
    result.trf_id =
        Specialization::template parse_integer<std::uint16_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "trf_id");
    result.trf_timestamp =
        Specialization::template parse_integer<std::uint64_t>(
            cursor.template next_field<Specialization, false>(scratch),
            "trf_timestamp");

    cursor.finish();
    return result;
  }

  template <std::size_t FieldCount>
  static std::array<std::string, FieldCount> parse_raw_row(std::string_view line) {
    std::vector<std::string> fields;
    Specialization::split_csv_fields(line, fields);
    detail::require_field_count("Raw CSV row", fields.size(), FieldCount);

    std::array<std::string, FieldCount> result;

    for (std::size_t index = 0; index < FieldCount; ++index) {
      result[index] = std::move(fields[index]);
    }

    return result;
  }

  template <std::size_t FieldCount>
  static nanobind::tuple make_raw_tuple() {
    nanobind::tuple result = nanobind::steal<nanobind::tuple>(PyTuple_New(FieldCount));
    if (!result.is_valid()) {
      throw nanobind::python_error();
    }
    return result;
  }

  static void set_raw_bytes_field(
      nanobind::tuple& result,
      std::size_t index,
      std::string_view field) {
    PyTuple_SET_ITEM(result.ptr(), index, detail::raw_bytes_new_ref(field));
  }

  static void set_raw_small_uint_field(
      nanobind::tuple& result,
      std::size_t index,
      std::string_view field,
      detail::RawBytesInternCache& intern_cache) {
    PyTuple_SET_ITEM(result.ptr(), index, intern_cache.small_uint_new_ref(field));
  }

  static void set_raw_ticker_field(
      nanobind::tuple& result,
      std::string_view ticker,
      detail::RawBytesInternCache& intern_cache) {
    PyTuple_SET_ITEM(result.ptr(), 0, intern_cache.ticker_new_ref(ticker));
  }

  template <bool ExpectMore>
  static std::string_view next_raw_unquoted_field(
      std::string_view line,
      std::size_t& cursor) {
    return Specialization::template parse_unquoted_field<ExpectMore>(line, cursor);
  }

  template <bool ExpectMore>
  static std::string_view next_raw_condition_field(
      std::string_view line,
      std::size_t& cursor,
      std::string& scratch) {
    if (cursor < line.size() && line[cursor] == '"') {
      return Specialization::template parse_quoted_field<ExpectMore>(
          line,
          cursor,
          scratch);
    }

    return Specialization::template parse_unquoted_field<ExpectMore>(line, cursor);
  }

  static void finish_raw_row(std::string_view line, std::size_t cursor) {
    if (cursor != line.size()) {
      throw std::invalid_argument("unexpected trailing data in CSV row");
    }
  }

  static RawStockTrade parse_raw_trade_row(std::string_view line) {
    return parse_raw_row<13>(line);
  }

  static nanobind::tuple parse_raw_trade_tuple(
      std::string_view line,
      detail::RawBytesInternCache& intern_cache) {
    std::size_t cursor = 0;
    std::string scratch;
    nanobind::tuple result = make_raw_tuple<13>();

    set_raw_ticker_field(
        result,
        next_raw_unquoted_field<true>(line, cursor),
        intern_cache);
    set_raw_bytes_field(result, 1, next_raw_condition_field<true>(line, cursor, scratch));
    set_raw_bytes_field(result, 2, next_raw_unquoted_field<true>(line, cursor));
    set_raw_small_uint_field(
        result,
        3,
        next_raw_unquoted_field<true>(line, cursor),
        intern_cache);
    set_raw_bytes_field(result, 4, next_raw_unquoted_field<true>(line, cursor));
    set_raw_bytes_field(result, 5, next_raw_unquoted_field<true>(line, cursor));
    set_raw_bytes_field(result, 6, next_raw_unquoted_field<true>(line, cursor));
    set_raw_bytes_field(result, 7, next_raw_unquoted_field<true>(line, cursor));
    set_raw_bytes_field(result, 8, next_raw_unquoted_field<true>(line, cursor));
    set_raw_bytes_field(result, 9, next_raw_unquoted_field<true>(line, cursor));
    set_raw_small_uint_field(
        result,
        10,
        next_raw_unquoted_field<true>(line, cursor),
        intern_cache);
    set_raw_bytes_field(result, 11, next_raw_unquoted_field<true>(line, cursor));
    set_raw_bytes_field(result, 12, next_raw_unquoted_field<false>(line, cursor));

    finish_raw_row(line, cursor);
    return result;
  }

  static nanobind::tuple raw_trade_array_to_tuple(
      const RawStockTrade& fields,
      detail::RawBytesInternCache& intern_cache) {
    nanobind::tuple result = make_raw_tuple<13>();
    set_raw_ticker_field(result, fields[0], intern_cache);
    set_raw_bytes_field(result, 1, fields[1]);
    set_raw_bytes_field(result, 2, fields[2]);
    set_raw_small_uint_field(result, 3, fields[3], intern_cache);
    set_raw_bytes_field(result, 4, fields[4]);
    set_raw_bytes_field(result, 5, fields[5]);
    set_raw_bytes_field(result, 6, fields[6]);
    set_raw_bytes_field(result, 7, fields[7]);
    set_raw_bytes_field(result, 8, fields[8]);
    set_raw_bytes_field(result, 9, fields[9]);
    set_raw_small_uint_field(result, 10, fields[10], intern_cache);
    set_raw_bytes_field(result, 11, fields[11]);
    set_raw_bytes_field(result, 12, fields[12]);
    return result;
  }

  static StockQuote parse_quote_row(std::string_view line) {
    detail::CsvLineCursor cursor(line);
    std::string scratch;
    StockQuote result;

    result.ticker.assign(cursor.template next_field<Specialization, true>(scratch));
    result.ask_exchange =
        Specialization::template parse_integer<std::uint8_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "ask_exchange");
    result.ask_price = Specialization::parse_double(
        cursor.template next_field<Specialization, true>(scratch),
        "ask_price");
    result.ask_size =
        Specialization::template parse_integer<std::uint32_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "ask_size");
    result.bid_exchange =
        Specialization::template parse_integer<std::uint8_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "bid_exchange");
    result.bid_price = Specialization::parse_double(
        cursor.template next_field<Specialization, true>(scratch),
        "bid_price");
    result.bid_size =
        Specialization::template parse_integer<std::uint32_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "bid_size");
    result.conditions =
        Specialization::template parse_bitset<96>(
            cursor.template next_field<Specialization, true>(scratch),
            "conditions");
    result.indicators =
        Specialization::template parse_bitset<96>(
            cursor.template next_field<Specialization, true>(scratch),
            "indicators");
    result.participant_timestamp =
        Specialization::template parse_integer<std::uint64_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "participant_timestamp");
    result.sequence_number =
        Specialization::template parse_integer<std::uint64_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "sequence_number");
    result.sip_timestamp =
        Specialization::template parse_integer<std::uint64_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "sip_timestamp");
    result.tape =
        Specialization::template parse_integer<std::uint8_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "tape");
    result.trf_timestamp =
        Specialization::template parse_integer<std::uint64_t>(
            cursor.template next_field<Specialization, false>(scratch),
            "trf_timestamp");

    cursor.finish();
    return result;
  }

  static RawStockQuote parse_raw_quote_row(std::string_view line) {
    return parse_raw_row<14>(line);
  }

  static nanobind::tuple parse_raw_quote_tuple(
      std::string_view line,
      detail::RawBytesInternCache& intern_cache) {
    std::size_t cursor = 0;
    std::string scratch;
    nanobind::tuple result = make_raw_tuple<14>();

    set_raw_ticker_field(
        result,
        next_raw_unquoted_field<true>(line, cursor),
        intern_cache);
    set_raw_small_uint_field(
        result,
        1,
        next_raw_unquoted_field<true>(line, cursor),
        intern_cache);
    set_raw_bytes_field(result, 2, next_raw_unquoted_field<true>(line, cursor));
    set_raw_bytes_field(result, 3, next_raw_unquoted_field<true>(line, cursor));
    set_raw_small_uint_field(
        result,
        4,
        next_raw_unquoted_field<true>(line, cursor),
        intern_cache);
    set_raw_bytes_field(result, 5, next_raw_unquoted_field<true>(line, cursor));
    set_raw_bytes_field(result, 6, next_raw_unquoted_field<true>(line, cursor));
    set_raw_bytes_field(result, 7, next_raw_condition_field<true>(line, cursor, scratch));
    set_raw_bytes_field(result, 8, next_raw_condition_field<true>(line, cursor, scratch));
    set_raw_bytes_field(result, 9, next_raw_unquoted_field<true>(line, cursor));
    set_raw_bytes_field(result, 10, next_raw_unquoted_field<true>(line, cursor));
    set_raw_bytes_field(result, 11, next_raw_unquoted_field<true>(line, cursor));
    set_raw_small_uint_field(
        result,
        12,
        next_raw_unquoted_field<true>(line, cursor),
        intern_cache);
    set_raw_bytes_field(result, 13, next_raw_unquoted_field<false>(line, cursor));

    finish_raw_row(line, cursor);
    return result;
  }

  static nanobind::tuple raw_quote_array_to_tuple(
      const RawStockQuote& fields,
      detail::RawBytesInternCache& intern_cache) {
    nanobind::tuple result = make_raw_tuple<14>();
    set_raw_ticker_field(result, fields[0], intern_cache);
    set_raw_small_uint_field(result, 1, fields[1], intern_cache);
    set_raw_bytes_field(result, 2, fields[2]);
    set_raw_bytes_field(result, 3, fields[3]);
    set_raw_small_uint_field(result, 4, fields[4], intern_cache);
    set_raw_bytes_field(result, 5, fields[5]);
    set_raw_bytes_field(result, 6, fields[6]);
    set_raw_bytes_field(result, 7, fields[7]);
    set_raw_bytes_field(result, 8, fields[8]);
    set_raw_bytes_field(result, 9, fields[9]);
    set_raw_bytes_field(result, 10, fields[10]);
    set_raw_bytes_field(result, 11, fields[11]);
    set_raw_small_uint_field(result, 12, fields[12], intern_cache);
    set_raw_bytes_field(result, 13, fields[13]);
    return result;
  }

  static void validate_sort_flags(
      bool sort_by_participant_timestamp,
      bool sort_by_sip_timestamp) {
    if (sort_by_participant_timestamp && sort_by_sip_timestamp) {
      throw std::invalid_argument(
          "sort_by_participant_timestamp and sort_by_sip_timestamp cannot both be true");
    }
  }

  template <typename RowType, typename StreamState>
  static RowType next_parsed_row(
      std::optional<StreamState>& stream_state,
      std::vector<RowType>& rows,
      std::size_t& row_index) {
    if (stream_state) {
      RowType row;
      if (stream_state->next_row(row)) {
        return row;
      }

      stream_state.reset();
      throw nanobind::stop_iteration();
    }

    if (row_index >= rows.size()) {
      throw nanobind::stop_iteration();
    }

    return std::move(rows[row_index++]);
  }

  static std::uint64_t participant_timestamp_value(const RawStockTrade& row) {
    return Specialization::template parse_integer<std::uint64_t>(
        row[5],
        "participant_timestamp");
  }

  static std::uint64_t participant_timestamp_value(const RawStockQuote& row) {
    return Specialization::template parse_integer<std::uint64_t>(
        row[9],
        "participant_timestamp");
  }

  template <typename RowType>
  static std::uint64_t participant_timestamp_value(const RowType& row) {
    return row.participant_timestamp;
  }

  static std::uint64_t sip_timestamp_value(const RawStockTrade& row) {
    return Specialization::template parse_integer<std::uint64_t>(row[8], "sip_timestamp");
  }

  static std::uint64_t sip_timestamp_value(const RawStockQuote& row) {
    return Specialization::template parse_integer<std::uint64_t>(row[11], "sip_timestamp");
  }

  template <typename RowType>
  static std::uint64_t sip_timestamp_value(const RowType& row) {
    return row.sip_timestamp;
  }

  static const std::string& ticker_value(const RawStockTrade& row) {
    return row[0];
  }

  static const std::string& ticker_value(const RawStockQuote& row) {
    return row[0];
  }

  template <typename RowType>
  static const std::string& ticker_value(const RowType& row) {
    return row.ticker;
  }

  template <typename RowType>
  static std::vector<RowType> collect_rows(
      const std::filesystem::path& path,
      bool sort_by_participant_timestamp,
      bool sort_by_sip_timestamp,
      ParseRowFn<RowType> parse_row) {
    validate_sort_flags(sort_by_participant_timestamp, sort_by_sip_timestamp);

    if (sort_by_participant_timestamp) {
      auto rows = load_rows(path, parse_row);
      std::stable_sort(rows.begin(), rows.end(), [](const RowType& lhs, const RowType& rhs) {
        const auto lhs_participant_timestamp = Implementation::participant_timestamp_value(lhs);
        const auto rhs_participant_timestamp = Implementation::participant_timestamp_value(rhs);
        if (lhs_participant_timestamp != rhs_participant_timestamp) {
          return lhs_participant_timestamp < rhs_participant_timestamp;
        }
        if (Implementation::ticker_value(lhs) != Implementation::ticker_value(rhs)) {
          return Implementation::ticker_value(lhs) < Implementation::ticker_value(rhs);
        }
        return Implementation::sip_timestamp_value(lhs) < Implementation::sip_timestamp_value(rhs);
      });
      return rows;
    }

    if (!sort_by_sip_timestamp) {
      return load_rows(path, parse_row);
    }

    return merge_flatfile_row_bins<RowType>(load_row_bins(path, parse_row));
  }

  template <typename RowType>
  static std::vector<RowType> load_rows(
      const std::filesystem::path& path,
      ParseRowFn<RowType> parse_row) {
    std::vector<RowType> rows;
    detail::BufferedGzipLineReader reader(path);
    std::string_view line;
    bool is_first_line = true;

    while (reader.template next_line<Specialization>(line)) {
      if (is_first_line) {
        is_first_line = false;
        continue;
      }

      if (line.empty()) {
        continue;
      }

      rows.emplace_back(parse_row(line));
    }

    return rows;
  }

  template <typename RowType>
  static std::vector<std::vector<RowType>> load_row_bins(
      const std::filesystem::path& path,
      ParseRowFn<RowType> parse_row) {
    std::vector<std::vector<RowType>> bins;
    detail::BufferedGzipLineReader reader(path);
    std::string_view line;
    bool is_first_line = true;

    while (reader.template next_line<Specialization>(line)) {
      if (is_first_line) {
        is_first_line = false;
        continue;
      }

      if (line.empty()) {
        continue;
      }

      RowType row = parse_row(line);
      if (bins.empty() || bins.back().empty() ||
          Implementation::ticker_value(bins.back().back()) != Implementation::ticker_value(row)) {
        bins.emplace_back();
      }
      bins.back().push_back(std::move(row));
    }

    return bins;
  }

  template <typename RowType>
  static std::vector<RowType> merge_flatfile_row_bins(std::vector<std::vector<RowType>> bins) {
    struct MergeNode {
      std::size_t bucket_index = 0;
      std::size_t row_index = 0;
    };

    struct Compare {
      const std::vector<std::vector<RowType>>* bins = nullptr;

      bool operator()(const MergeNode& lhs, const MergeNode& rhs) const {
        const RowType& left = (*bins)[lhs.bucket_index][lhs.row_index];
        const RowType& right = (*bins)[rhs.bucket_index][rhs.row_index];

        const auto left_sip_timestamp = Implementation::sip_timestamp_value(left);
        const auto right_sip_timestamp = Implementation::sip_timestamp_value(right);
        if (left_sip_timestamp != right_sip_timestamp) {
          return left_sip_timestamp > right_sip_timestamp;
        }
        if (Implementation::ticker_value(left) != Implementation::ticker_value(right)) {
          return Implementation::ticker_value(left) > Implementation::ticker_value(right);
        }
        return Implementation::participant_timestamp_value(left) >
            Implementation::participant_timestamp_value(right);
      }
    };

    std::size_t total_rows = 0;
    for (const auto& bucket : bins) {
      total_rows += bucket.size();
    }

    std::vector<RowType> merged;
    merged.reserve(total_rows);

    std::priority_queue<MergeNode, std::vector<MergeNode>, Compare> queue{
        Compare{&bins},
        {},
    };

    for (std::size_t bucket_index = 0; bucket_index < bins.size(); ++bucket_index) {
      if (!bins[bucket_index].empty()) {
        queue.push(MergeNode{bucket_index, 0});
      }
    }

    while (!queue.empty()) {
      const MergeNode node = queue.top();
      queue.pop();

      merged.push_back(std::move(bins[node.bucket_index][node.row_index]));

      const std::size_t next_row_index = node.row_index + 1;
      if (next_row_index < bins[node.bucket_index].size()) {
        queue.push(MergeNode{node.bucket_index, next_row_index});
      }
    }

    return merged;
  }
};

}  // namespace massive_speedup::backend_generic
