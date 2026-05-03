#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <bitset>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <generator>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#if __has_include(<rapidgzip/ParallelGzipReader.hpp>) && __has_include(<filereader/Standard.hpp>)
  #include <filereader/Standard.hpp>
  #include <rapidgzip/ParallelGzipReader.hpp>
  #define MASSIVE_SPEEDUP_HAS_RAPIDGZIP_HEADERS 1
#else
  #define MASSIVE_SPEEDUP_HAS_RAPIDGZIP_HEADERS 0
#endif

#include "massive_speedup/parsers.hpp"

namespace massive_speedup::native {

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

template <std::size_t PackedSize>
using PackedBuffer = std::array<std::uint8_t, PackedSize>;

inline void require_packed_size(
    std::string_view row_name,
    std::size_t actual,
    std::size_t expected) {
  if (actual != expected) {
    std::ostringstream message;
    message << row_name << " packed data expected " << expected << " bytes, received "
            << actual;
    throw std::invalid_argument(message.str());
  }
}

template <std::size_t PackedSize>
nanobind::bytes packed_bytes(const PackedBuffer<PackedSize>& data) {
  return nanobind::bytes(
      reinterpret_cast<const char*>(data.data()),
      data.size());
}

inline std::string utc_date_directory_name(std::uint64_t timestamp_ns) {
  const auto seconds_since_epoch =
      std::chrono::seconds(timestamp_ns / 1'000'000'000ULL);
  const auto day = std::chrono::floor<std::chrono::days>(
      std::chrono::sys_seconds(seconds_since_epoch));
  const std::chrono::year_month_day ymd(day);

  std::array<char, 11> output{};
  std::snprintf(
      output.data(),
      output.size(),
      "%04d-%02u-%02u",
      static_cast<int>(ymd.year()),
      static_cast<unsigned>(ymd.month()),
      static_cast<unsigned>(ymd.day()));
  return std::string(output.data(), 10);
}

class BinaryRecordWriter {
 public:
  explicit BinaryRecordWriter(std::size_t buffer_size = 1U << 20)
      : buffer_(buffer_size) {}

  ~BinaryRecordWriter() {
    if (file_ != nullptr) {
      std::fclose(file_);
    }
  }

  BinaryRecordWriter(const BinaryRecordWriter&) = delete;
  BinaryRecordWriter& operator=(const BinaryRecordWriter&) = delete;

  void open(const std::filesystem::path& path) {
    close();
    const std::string filename = path.string();
    file_ = std::fopen(filename.c_str(), "wb");
    if (file_ == nullptr) {
      std::ostringstream message;
      message << "unable to open database output file " << path << ": "
              << std::strerror(errno);
      throw std::runtime_error(message.str());
    }

    if (!buffer_.empty()) {
      std::setvbuf(file_, buffer_.data(), _IOFBF, buffer_.size());
    }
  }

  template <std::size_t PackedSize>
  void write(const PackedBuffer<PackedSize>& data) {
    if (file_ == nullptr) {
      throw std::logic_error("database output file is not open");
    }

    const std::size_t written = std::fwrite(data.data(), 1, data.size(), file_);
    if (written != data.size()) {
      throw std::runtime_error("failed to write packed database record");
    }
  }

  void close() {
    if (file_ == nullptr) {
      return;
    }

    FILE* file = std::exchange(file_, nullptr);
    if (std::fclose(file) != 0) {
      std::ostringstream message;
      message << "failed to close database output file: " << std::strerror(errno);
      throw std::runtime_error(message.str());
    }
  }

 private:
  std::vector<char> buffer_;
  FILE* file_ = nullptr;
};

class MappedFile {
 public:
  explicit MappedFile(std::filesystem::path path)
      : path_(std::move(path)) {
    const std::string filename = path_.string();
    const int fd = ::open(filename.c_str(), O_RDONLY);
    if (fd < 0) {
      std::ostringstream message;
      message << "unable to open database file " << path_ << ": " << std::strerror(errno);
      throw std::runtime_error(message.str());
    }

    struct stat status {};
    if (::fstat(fd, &status) != 0) {
      const int saved_errno = errno;
      ::close(fd);
      std::ostringstream message;
      message << "unable to stat database file " << path_ << ": "
              << std::strerror(saved_errno);
      throw std::runtime_error(message.str());
    }

    if (status.st_size < 0) {
      ::close(fd);
      throw std::runtime_error("database file size is negative");
    }

    size_ = static_cast<std::size_t>(status.st_size);
    if (size_ == 0) {
      ::close(fd);
      return;
    }

    void* mapped = ::mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd, 0);
    const int saved_errno = errno;
    ::close(fd);
    if (mapped == MAP_FAILED) {
      std::ostringstream message;
      message << "unable to mmap database file " << path_ << ": "
              << std::strerror(saved_errno);
      throw std::runtime_error(message.str());
    }

    data_ = static_cast<const std::byte*>(mapped);
  }

  ~MappedFile() {
    if (data_ != nullptr) {
      ::munmap(const_cast<std::byte*>(data_), size_);
    }
  }

  MappedFile(const MappedFile&) = delete;
  MappedFile& operator=(const MappedFile&) = delete;

  MappedFile(MappedFile&& other) noexcept
      : path_(std::move(other.path_)),
        data_(std::exchange(other.data_, nullptr)),
        size_(std::exchange(other.size_, 0)) {}

  MappedFile& operator=(MappedFile&& other) noexcept {
    if (this == &other) {
      return *this;
    }

    if (data_ != nullptr) {
      ::munmap(const_cast<std::byte*>(data_), size_);
    }

    path_ = std::move(other.path_);
    data_ = std::exchange(other.data_, nullptr);
    size_ = std::exchange(other.size_, 0);
    return *this;
  }

  const void* data_at(std::size_t offset) const {
    return data_ + offset;
  }

  const char* char_data_at(std::size_t offset) const {
    return reinterpret_cast<const char*>(data_ + offset);
  }

  std::size_t size() const { return size_; }
  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
  const std::byte* data_ = nullptr;
  std::size_t size_ = 0;
};

template <typename UIntType, std::size_t PackedSize>
void write_unsigned_le(
    PackedBuffer<PackedSize>& output,
    std::size_t& offset,
    UIntType value) {
  static_assert(std::is_unsigned_v<UIntType>);
  for (std::size_t byte_index = 0; byte_index < sizeof(UIntType); ++byte_index) {
    output[offset++] = static_cast<std::uint8_t>(
        (value >> (byte_index * 8U)) & static_cast<UIntType>(0xffU));
  }
}

template <typename UIntType>
UIntType read_unsigned_le(std::string_view input, std::size_t& offset) {
  static_assert(std::is_unsigned_v<UIntType>);
  UIntType value = 0;
  for (std::size_t byte_index = 0; byte_index < sizeof(UIntType); ++byte_index) {
    const auto byte = static_cast<UIntType>(
        static_cast<unsigned char>(input[offset++]));
    value |= static_cast<UIntType>(byte << (byte_index * 8U));
  }
  return value;
}

inline std::uint64_t read_uint64_le_at(const void* data, std::size_t offset) {
  std::uint64_t value = 0;
  std::memcpy(
      &value,
      static_cast<const std::uint8_t*>(data) + offset,
      sizeof(value));
  if constexpr (std::endian::native == std::endian::little) {
    return value;
  } else {
    return std::byteswap(value);
  }
}

inline std::uint32_t read_uint32_le_at(const void* data, std::size_t offset) {
  std::uint32_t value = 0;
  std::memcpy(
      &value,
      static_cast<const std::uint8_t*>(data) + offset,
      sizeof(value));
  if constexpr (std::endian::native == std::endian::little) {
    return value;
  } else {
    return std::byteswap(value);
  }
}

inline std::int32_t read_int32_le_at(const void* data, std::size_t offset) {
  return std::bit_cast<std::int32_t>(read_uint32_le_at(data, offset));
}

inline double read_double_le_at(const void* data, std::size_t offset) {
  static_assert(sizeof(double) == sizeof(std::uint64_t));
  static_assert(std::numeric_limits<double>::is_iec559);
  return std::bit_cast<double>(read_uint64_le_at(data, offset));
}

template <std::size_t PackedSize>
void write_int32_le(
    PackedBuffer<PackedSize>& output,
    std::size_t& offset,
    std::int32_t value) {
  write_unsigned_le(output, offset, std::bit_cast<std::uint32_t>(value));
}

inline std::int32_t read_int32_le(std::string_view input, std::size_t& offset) {
  return std::bit_cast<std::int32_t>(read_unsigned_le<std::uint32_t>(input, offset));
}

template <std::size_t PackedSize>
void write_double_le(
    PackedBuffer<PackedSize>& output,
    std::size_t& offset,
    double value) {
  static_assert(sizeof(double) == sizeof(std::uint64_t));
  static_assert(std::numeric_limits<double>::is_iec559);
  write_unsigned_le(output, offset, std::bit_cast<std::uint64_t>(value));
}

inline double read_double_le(std::string_view input, std::size_t& offset) {
  static_assert(sizeof(double) == sizeof(std::uint64_t));
  static_assert(std::numeric_limits<double>::is_iec559);
  return std::bit_cast<double>(read_unsigned_le<std::uint64_t>(input, offset));
}

template <std::size_t BitCount, std::size_t PackedSize>
void write_bitset_le(
    PackedBuffer<PackedSize>& output,
    std::size_t& offset,
    const std::bitset<BitCount>& bits) {
  static_assert(BitCount % 8 == 0);
  for (std::size_t byte_index = 0; byte_index < BitCount / 8; ++byte_index) {
    std::uint8_t byte = 0;
    for (std::size_t bit_index = 0; bit_index < 8; ++bit_index) {
      if (bits.test(byte_index * 8 + bit_index)) {
        byte |= static_cast<std::uint8_t>(1U << bit_index);
      }
    }
    output[offset++] = byte;
  }
}

template <std::size_t BitCount>
std::bitset<BitCount> read_bitset_le(std::string_view input, std::size_t& offset) {
  static_assert(BitCount % 8 == 0);
  std::bitset<BitCount> bits;
  for (std::size_t byte_index = 0; byte_index < BitCount / 8; ++byte_index) {
    const auto byte = static_cast<unsigned char>(input[offset++]);
    for (std::size_t bit_index = 0; bit_index < 8; ++bit_index) {
      if ((byte & (1U << bit_index)) != 0) {
        bits.set(byte_index * 8 + bit_index);
      }
    }
  }
  return bits;
}

struct TransparentStringHash {
  using is_transparent = void;

  std::size_t operator()(std::string_view value) const noexcept {
    return std::hash<std::string_view>{}(value);
  }

  std::size_t operator()(const std::string& value) const noexcept {
    return std::hash<std::string_view>{}(value);
  }
};

struct TransparentStringEqual {
  using is_transparent = void;

  bool operator()(std::string_view lhs, std::string_view rhs) const noexcept {
    return lhs == rhs;
  }
};

template <std::size_t BitCount>
class BitsetParseCache {
 public:
  const std::bitset<BitCount>& get_or_parse(
      std::string_view text,
      std::string_view field_name) {
    const auto found = cache_.find(text);
    if (found != cache_.end()) {
      return found->second;
    }

    auto [iter, inserted] = cache_.emplace(
        std::string(text),
        parse_bitset<BitCount>(text, field_name));
    static_cast<void>(inserted);
    return iter->second;
  }

 private:
  std::unordered_map<
      std::string,
      std::bitset<BitCount>,
      TransparentStringHash,
      TransparentStringEqual>
      cache_;
};

template <std::size_t BitCount>
inline std::size_t bitset_hash(const std::bitset<BitCount>& bits) {
  std::size_t seed = 0;

  for (std::size_t base = 0; base < BitCount; base += 64) {
    std::uint64_t chunk = 0;
    const std::size_t limit = std::min<std::size_t>(64, BitCount - base);

    for (std::size_t offset = 0; offset < limit; ++offset) {
      if (bits.test(base + offset)) {
        chunk |= (std::uint64_t{1} << offset);
      }
    }

    seed ^= std::hash<std::uint64_t>{}(chunk) + 0x9e3779b97f4a7c15ULL + (seed << 6U) +
        (seed >> 2U);
  }

  return seed;
}

inline nanobind::object bit_indices_frozenset(const std::bitset<96>& bits) {
  struct BitsetKeyHash {
    std::size_t operator()(const std::bitset<96>& value) const noexcept {
      return bitset_hash(value);
    }
  };

  using InternTable = std::unordered_map<std::bitset<96>, PyObject*, BitsetKeyHash>;
  static InternTable* interned_sets = new InternTable();

  if (const auto found = interned_sets->find(bits); found != interned_sets->end()) {
    Py_INCREF(found->second);
    return nanobind::steal<nanobind::object>(found->second);
  }

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

  Py_INCREF(result.ptr());
  interned_sets->emplace(bits, result.ptr());
  return result;
}

inline PyObject* intern_unicode_from_view(std::string_view value) {
  PyObject* unicode = PyUnicode_FromStringAndSize(value.data(), static_cast<Py_ssize_t>(value.size()));
  if (unicode == nullptr) {
    throw nanobind::python_error();
  }

  PyUnicode_InternInPlace(&unicode);
  if (unicode == nullptr) {
    throw nanobind::python_error();
  }
  return unicode;
}

inline nanobind::object currency_tickers_tuple(std::string_view ticker) {
  using InternTable = std::unordered_map<
      std::string,
      PyObject*,
      TransparentStringHash,
      TransparentStringEqual>;
  static InternTable* interned_tickers = new InternTable();

  if (const auto found = interned_tickers->find(ticker); found != interned_tickers->end()) {
    Py_INCREF(found->second);
    return nanobind::steal<nanobind::object>(found->second);
  }

  const auto colon = ticker.find(':');
  const std::string_view symbol = colon == std::string_view::npos ? ticker : ticker.substr(colon + 1);
  const auto dash = symbol.find('-');
  const std::string_view base = dash == std::string_view::npos ? symbol : symbol.substr(0, dash);
  const std::string_view quote = dash == std::string_view::npos ? std::string_view{} : symbol.substr(dash + 1);

  nanobind::tuple result = nanobind::steal<nanobind::tuple>(PyTuple_New(2));
  if (!result.is_valid()) {
    throw nanobind::python_error();
  }

  PyTuple_SET_ITEM(result.ptr(), 0, intern_unicode_from_view(base));
  PyTuple_SET_ITEM(result.ptr(), 1, intern_unicode_from_view(quote));

  Py_INCREF(result.ptr());
  interned_tickers->emplace(std::string(ticker), result.ptr());
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

template <std::size_t Count>
class LazyPythonObjectCache {
 public:
  LazyPythonObjectCache() = default;

  ~LazyPythonObjectCache() {
    for (PyObject* object : objects_) {
      Py_XDECREF(object);
    }
  }

  LazyPythonObjectCache(const LazyPythonObjectCache&) = delete;
  LazyPythonObjectCache& operator=(const LazyPythonObjectCache&) = delete;

  template <typename Factory>
  nanobind::object get(std::size_t index, Factory&& factory) {
    PyObject*& cached = objects_[index];
    if (cached == nullptr) {
      cached = factory();
      if (cached == nullptr) {
        throw nanobind::python_error();
      }
    }

    Py_INCREF(cached);
    return nanobind::steal<nanobind::object>(cached);
  }

 private:
  std::array<PyObject*, Count> objects_{};
};

template <std::size_t Count, typename Factory>
nanobind::object cached_python_object(
    std::unique_ptr<LazyPythonObjectCache<Count>>& cache,
    std::size_t index,
    Factory&& factory) {
  if (!cache) {
    cache = std::make_unique<LazyPythonObjectCache<Count>>();
  }

  return cache->get(index, std::forward<Factory>(factory));
}

inline PyObject* string_object_new_ref(const std::string& value) {
  return PyUnicode_FromStringAndSize(
      value.data(),
      static_cast<Py_ssize_t>(value.size()));
}

inline PyObject* uint64_object_new_ref(std::uint64_t value) {
  return PyLong_FromUnsignedLongLong(static_cast<unsigned long long>(value));
}

inline PyObject* int64_object_new_ref(std::int64_t value) {
  return PyLong_FromLongLong(static_cast<long long>(value));
}

inline PyObject* double_object_new_ref(double value) {
  return PyFloat_FromDouble(value);
}

inline PyObject* object_cache_new_ref(nanobind::object object) {
  PyObject* pointer = object.ptr();
  Py_INCREF(pointer);
  return pointer;
}

template <std::size_t Count>
class EmbeddedPythonObjectCache {
 public:
  EmbeddedPythonObjectCache() = default;

  ~EmbeddedPythonObjectCache() {
    clear();
  }

  EmbeddedPythonObjectCache(const EmbeddedPythonObjectCache&) {}

  EmbeddedPythonObjectCache& operator=(const EmbeddedPythonObjectCache&) {
    clear();
    return *this;
  }

  EmbeddedPythonObjectCache(EmbeddedPythonObjectCache&&) noexcept {}

  EmbeddedPythonObjectCache& operator=(EmbeddedPythonObjectCache&&) noexcept {
    clear();
    return *this;
  }

  void clear() const {
    for (PyObject*& object : objects_) {
      Py_XDECREF(object);
      object = nullptr;
    }
  }

  template <typename Factory>
  nanobind::object get(std::size_t index, Factory&& factory) const {
    PyObject*& cached = objects_[index];
    if (cached == nullptr) {
      cached = factory();
      if (cached == nullptr) {
        throw nanobind::python_error();
      }
    }

    Py_INCREF(cached);
    return nanobind::steal<nanobind::object>(cached);
  }

 private:
  mutable std::array<PyObject*, Count> objects_{};
};

template <std::size_t Count>
class AggregateObjectCache {
 public:
  nanobind::object cached_string(
      std::size_t index,
      const std::string& value) const {
    return object_cache_.get(
        index,
        [&] { return string_object_new_ref(value); });
  }

  nanobind::object cached_double(std::size_t index, double value) const {
    return object_cache_.get(
        index,
        [&] { return double_object_new_ref(value); });
  }

  nanobind::object cached_uint64(std::size_t index, std::uint64_t value) const {
    return object_cache_.get(
        index,
        [&] { return uint64_object_new_ref(value); });
  }

 private:
  EmbeddedPythonObjectCache<Count> object_cache_;
};

}  // namespace detail

struct StockTrade {
  static constexpr std::size_t packed_size = 73;
  static constexpr std::size_t packed_participant_timestamp_offset = 25;
  static constexpr std::size_t packed_price_offset = 33;
  static constexpr std::size_t packed_sip_timestamp_offset = 49;
  static constexpr std::size_t packed_size_offset = 57;
  using PackedData = detail::PackedBuffer<packed_size>;
  enum AttributeIndex : std::size_t {
    ticker_attribute,
    conditions_attribute,
    correction_attribute,
    exchange_attribute,
    id_attribute,
    participant_timestamp_attribute,
    price_attribute,
    sequence_number_attribute,
    sip_timestamp_attribute,
    size_attribute,
    tape_attribute,
    trf_id_attribute,
    trf_timestamp_attribute,
    attribute_count,
  };

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
  mutable std::unique_ptr<detail::LazyPythonObjectCache<attribute_count>> object_cache_;

  StockTrade() = default;

  StockTrade(const StockTrade& other)
      : ticker(other.ticker),
        conditions(other.conditions),
        price(other.price),
        id(other.id),
        participant_timestamp(other.participant_timestamp),
        sequence_number(other.sequence_number),
        sip_timestamp(other.sip_timestamp),
        trf_timestamp(other.trf_timestamp),
        correction(other.correction),
        size(other.size),
        tape(other.tape),
        trf_id(other.trf_id),
        exchange(other.exchange) {}

  StockTrade& operator=(const StockTrade& other) {
    if (this == &other) {
      return *this;
    }

    ticker = other.ticker;
    conditions = other.conditions;
    price = other.price;
    id = other.id;
    participant_timestamp = other.participant_timestamp;
    sequence_number = other.sequence_number;
    sip_timestamp = other.sip_timestamp;
    trf_timestamp = other.trf_timestamp;
    correction = other.correction;
    size = other.size;
    tape = other.tape;
    trf_id = other.trf_id;
    exchange = other.exchange;
    object_cache_.reset();
    return *this;
  }

  StockTrade(StockTrade&&) noexcept = default;
  StockTrade& operator=(StockTrade&&) noexcept = default;

  StockTrade(std::string_view packed_data, std::string_view ticker_value) {
    *this = from_packed(packed_data, ticker_value);
  }

  StockTrade(const char* packed_data, std::string_view ticker_value)
      : StockTrade(std::string_view(packed_data, packed_size), ticker_value) {}

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

  static StockTrade from_packed(std::string_view packed_data) {
    detail::require_packed_size("StockTrade", packed_data.size(), packed_size);

    StockTrade result;
    std::size_t offset = 0;
    result.conditions = detail::read_bitset_le<96>(packed_data, offset);
    result.correction = detail::read_int32_le(packed_data, offset);
    result.exchange = detail::read_unsigned_le<std::uint8_t>(packed_data, offset);
    result.id = detail::read_unsigned_le<std::uint64_t>(packed_data, offset);
    result.participant_timestamp =
        detail::read_unsigned_le<std::uint64_t>(packed_data, offset);
    result.price = detail::read_double_le(packed_data, offset);
    result.sequence_number = detail::read_unsigned_le<std::uint64_t>(packed_data, offset);
    result.sip_timestamp = detail::read_unsigned_le<std::uint64_t>(packed_data, offset);
    result.size = detail::read_int32_le(packed_data, offset);
    result.tape = detail::read_unsigned_le<std::uint16_t>(packed_data, offset);
    result.trf_id = detail::read_unsigned_le<std::uint16_t>(packed_data, offset);
    result.trf_timestamp = detail::read_unsigned_le<std::uint64_t>(packed_data, offset);
    return result;
  }

  static StockTrade from_packed(std::string_view packed_data, std::string_view ticker_value) {
    StockTrade result = from_packed(packed_data);
    result.ticker.assign(ticker_value);
    return result;
  }

  static StockTrade from_packed_data(const char* packed_data, std::string_view ticker_value) {
    return from_packed(std::string_view(packed_data, packed_size), ticker_value);
  }

  static std::uint64_t participant_timestamp_at(const void* packed_data) {
    return detail::read_uint64_le_at(
        packed_data,
        packed_participant_timestamp_offset);
  }

  static std::uint64_t sip_timestamp_at(const void* packed_data) {
    return detail::read_uint64_le_at(packed_data, packed_sip_timestamp_offset);
  }

  static double price_at(const void* packed_data) {
    return detail::read_double_le_at(packed_data, packed_price_offset);
  }

  static std::int32_t size_at(const void* packed_data) {
    return detail::read_int32_le_at(packed_data, packed_size_offset);
  }

  PackedData pack() const {
    PackedData output{};
    std::size_t offset = 0;
    detail::write_bitset_le(output, offset, conditions);
    detail::write_int32_le(output, offset, correction);
    detail::write_unsigned_le(output, offset, exchange);
    detail::write_unsigned_le(output, offset, id);
    detail::write_unsigned_le(output, offset, participant_timestamp);
    detail::write_double_le(output, offset, price);
    detail::write_unsigned_le(output, offset, sequence_number);
    detail::write_unsigned_le(output, offset, sip_timestamp);
    detail::write_int32_le(output, offset, size);
    detail::write_unsigned_le(output, offset, tape);
    detail::write_unsigned_le(output, offset, trf_id);
    detail::write_unsigned_le(output, offset, trf_timestamp);
    return output;
  }

  nanobind::bytes packed_bytes() const {
    return detail::packed_bytes(pack());
  }

  bool operator==(const StockTrade& other) const {
    return ticker == other.ticker &&
           conditions == other.conditions &&
           correction == other.correction &&
           exchange == other.exchange &&
           id == other.id &&
           participant_timestamp == other.participant_timestamp &&
           price == other.price &&
           sequence_number == other.sequence_number &&
           sip_timestamp == other.sip_timestamp &&
           size == other.size &&
           tape == other.tape &&
           trf_id == other.trf_id &&
           trf_timestamp == other.trf_timestamp;
  }

  nanobind::object ticker_object() const {
    return detail::cached_python_object(
        object_cache_,
        ticker_attribute,
        [&] { return detail::string_object_new_ref(ticker); });
  }

  nanobind::object conditions_object() const {
    return detail::cached_python_object(
        object_cache_,
        conditions_attribute,
        [&] {
          return detail::object_cache_new_ref(
              detail::bit_indices_frozenset(conditions));
        });
  }

  nanobind::object correction_object() const {
    return detail::cached_python_object(
        object_cache_,
        correction_attribute,
        [&] { return detail::int64_object_new_ref(correction); });
  }

  nanobind::object exchange_object() const {
    return detail::cached_python_object(
        object_cache_,
        exchange_attribute,
        [&] { return detail::uint64_object_new_ref(exchange); });
  }

  nanobind::object id_object() const {
    return detail::cached_python_object(
        object_cache_,
        id_attribute,
        [&] { return detail::uint64_object_new_ref(id); });
  }

  nanobind::object participant_timestamp_object() const {
    return detail::cached_python_object(
        object_cache_,
        participant_timestamp_attribute,
        [&] { return detail::uint64_object_new_ref(participant_timestamp); });
  }

  nanobind::object price_object() const {
    return detail::cached_python_object(
        object_cache_,
        price_attribute,
        [&] { return detail::double_object_new_ref(price); });
  }

  nanobind::object sequence_number_object() const {
    return detail::cached_python_object(
        object_cache_,
        sequence_number_attribute,
        [&] { return detail::uint64_object_new_ref(sequence_number); });
  }

  nanobind::object sip_timestamp_object() const {
    return detail::cached_python_object(
        object_cache_,
        sip_timestamp_attribute,
        [&] { return detail::uint64_object_new_ref(sip_timestamp); });
  }

  nanobind::object size_object() const {
    return detail::cached_python_object(
        object_cache_,
        size_attribute,
        [&] { return detail::int64_object_new_ref(size); });
  }

  nanobind::object tape_object() const {
    return detail::cached_python_object(
        object_cache_,
        tape_attribute,
        [&] { return detail::uint64_object_new_ref(tape); });
  }

  nanobind::object trf_id_object() const {
    return detail::cached_python_object(
        object_cache_,
        trf_id_attribute,
        [&] { return detail::uint64_object_new_ref(trf_id); });
  }

  nanobind::object trf_timestamp_object() const {
    return detail::cached_python_object(
        object_cache_,
        trf_timestamp_attribute,
        [&] { return detail::uint64_object_new_ref(trf_timestamp); });
  }

  nanobind::list python_fields() const {
    nanobind::list values;
    values.append(ticker_object());
    values.append(conditions_object());
    values.append(correction_object());
    values.append(exchange_object());
    values.append(id_object());
    values.append(participant_timestamp_object());
    values.append(price_object());
    values.append(sequence_number_object());
    values.append(sip_timestamp_object());
    values.append(size_object());
    values.append(tape_object());
    values.append(trf_id_object());
    values.append(trf_timestamp_object());
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
  static constexpr std::size_t packed_size = 83;
  static constexpr std::size_t packed_ask_price_offset = 1;
  static constexpr std::size_t packed_ask_size_offset = 9;
  static constexpr std::size_t packed_bid_price_offset = 14;
  static constexpr std::size_t packed_bid_size_offset = 22;
  static constexpr std::size_t packed_participant_timestamp_offset = 50;
  static constexpr std::size_t packed_sip_timestamp_offset = 66;
  using PackedData = detail::PackedBuffer<packed_size>;
  enum AttributeIndex : std::size_t {
    ticker_attribute,
    ask_exchange_attribute,
    ask_price_attribute,
    ask_size_attribute,
    bid_exchange_attribute,
    bid_price_attribute,
    bid_size_attribute,
    conditions_attribute,
    indicators_attribute,
    participant_timestamp_attribute,
    sequence_number_attribute,
    sip_timestamp_attribute,
    tape_attribute,
    trf_timestamp_attribute,
    attribute_count,
  };

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
  mutable std::unique_ptr<detail::LazyPythonObjectCache<attribute_count>> object_cache_;

  StockQuote() = default;

  StockQuote(const StockQuote& other)
      : ticker(other.ticker),
        conditions(other.conditions),
        indicators(other.indicators),
        ask_price(other.ask_price),
        bid_price(other.bid_price),
        participant_timestamp(other.participant_timestamp),
        sequence_number(other.sequence_number),
        sip_timestamp(other.sip_timestamp),
        trf_timestamp(other.trf_timestamp),
        ask_size(other.ask_size),
        bid_size(other.bid_size),
        ask_exchange(other.ask_exchange),
        bid_exchange(other.bid_exchange),
        tape(other.tape) {}

  StockQuote& operator=(const StockQuote& other) {
    if (this == &other) {
      return *this;
    }

    ticker = other.ticker;
    conditions = other.conditions;
    indicators = other.indicators;
    ask_price = other.ask_price;
    bid_price = other.bid_price;
    participant_timestamp = other.participant_timestamp;
    sequence_number = other.sequence_number;
    sip_timestamp = other.sip_timestamp;
    trf_timestamp = other.trf_timestamp;
    ask_size = other.ask_size;
    bid_size = other.bid_size;
    ask_exchange = other.ask_exchange;
    bid_exchange = other.bid_exchange;
    tape = other.tape;
    object_cache_.reset();
    return *this;
  }

  StockQuote(StockQuote&&) noexcept = default;
  StockQuote& operator=(StockQuote&&) noexcept = default;

  StockQuote(std::string_view packed_data, std::string_view ticker_value) {
    *this = from_packed(packed_data, ticker_value);
  }

  StockQuote(const char* packed_data, std::string_view ticker_value)
      : StockQuote(std::string_view(packed_data, packed_size), ticker_value) {}

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

  static StockQuote from_packed(std::string_view packed_data) {
    detail::require_packed_size("StockQuote", packed_data.size(), packed_size);

    StockQuote result;
    std::size_t offset = 0;
    result.ask_exchange = detail::read_unsigned_le<std::uint8_t>(packed_data, offset);
    result.ask_price = detail::read_double_le(packed_data, offset);
    result.ask_size = detail::read_unsigned_le<std::uint32_t>(packed_data, offset);
    result.bid_exchange = detail::read_unsigned_le<std::uint8_t>(packed_data, offset);
    result.bid_price = detail::read_double_le(packed_data, offset);
    result.bid_size = detail::read_unsigned_le<std::uint32_t>(packed_data, offset);
    result.conditions = detail::read_bitset_le<96>(packed_data, offset);
    result.indicators = detail::read_bitset_le<96>(packed_data, offset);
    result.participant_timestamp =
        detail::read_unsigned_le<std::uint64_t>(packed_data, offset);
    result.sequence_number = detail::read_unsigned_le<std::uint64_t>(packed_data, offset);
    result.sip_timestamp = detail::read_unsigned_le<std::uint64_t>(packed_data, offset);
    result.tape = detail::read_unsigned_le<std::uint8_t>(packed_data, offset);
    result.trf_timestamp = detail::read_unsigned_le<std::uint64_t>(packed_data, offset);
    return result;
  }

  static StockQuote from_packed(std::string_view packed_data, std::string_view ticker_value) {
    StockQuote result = from_packed(packed_data);
    result.ticker.assign(ticker_value);
    return result;
  }

  static StockQuote from_packed_data(const char* packed_data, std::string_view ticker_value) {
    return from_packed(std::string_view(packed_data, packed_size), ticker_value);
  }

  static std::uint64_t participant_timestamp_at(const void* packed_data) {
    return detail::read_uint64_le_at(
        packed_data,
        packed_participant_timestamp_offset);
  }

  static std::uint64_t sip_timestamp_at(const void* packed_data) {
    return detail::read_uint64_le_at(packed_data, packed_sip_timestamp_offset);
  }

  static double ask_price_at(const void* packed_data) {
    return detail::read_double_le_at(packed_data, packed_ask_price_offset);
  }

  static std::uint32_t ask_size_at(const void* packed_data) {
    return detail::read_uint32_le_at(packed_data, packed_ask_size_offset);
  }

  static double bid_price_at(const void* packed_data) {
    return detail::read_double_le_at(packed_data, packed_bid_price_offset);
  }

  static std::uint32_t bid_size_at(const void* packed_data) {
    return detail::read_uint32_le_at(packed_data, packed_bid_size_offset);
  }

  PackedData pack() const {
    PackedData output{};
    std::size_t offset = 0;
    detail::write_unsigned_le(output, offset, ask_exchange);
    detail::write_double_le(output, offset, ask_price);
    detail::write_unsigned_le(output, offset, ask_size);
    detail::write_unsigned_le(output, offset, bid_exchange);
    detail::write_double_le(output, offset, bid_price);
    detail::write_unsigned_le(output, offset, bid_size);
    detail::write_bitset_le(output, offset, conditions);
    detail::write_bitset_le(output, offset, indicators);
    detail::write_unsigned_le(output, offset, participant_timestamp);
    detail::write_unsigned_le(output, offset, sequence_number);
    detail::write_unsigned_le(output, offset, sip_timestamp);
    detail::write_unsigned_le(output, offset, tape);
    detail::write_unsigned_le(output, offset, trf_timestamp);
    return output;
  }

  nanobind::bytes packed_bytes() const {
    return detail::packed_bytes(pack());
  }

  bool operator==(const StockQuote& other) const {
    return ticker == other.ticker &&
           ask_exchange == other.ask_exchange &&
           ask_price == other.ask_price &&
           ask_size == other.ask_size &&
           bid_exchange == other.bid_exchange &&
           bid_price == other.bid_price &&
           bid_size == other.bid_size &&
           conditions == other.conditions &&
           indicators == other.indicators &&
           participant_timestamp == other.participant_timestamp &&
           sequence_number == other.sequence_number &&
           sip_timestamp == other.sip_timestamp &&
           tape == other.tape &&
           trf_timestamp == other.trf_timestamp;
  }

  nanobind::object ticker_object() const {
    return detail::cached_python_object(
        object_cache_,
        ticker_attribute,
        [&] { return detail::string_object_new_ref(ticker); });
  }

  nanobind::object ask_exchange_object() const {
    return detail::cached_python_object(
        object_cache_,
        ask_exchange_attribute,
        [&] { return detail::uint64_object_new_ref(ask_exchange); });
  }

  nanobind::object ask_price_object() const {
    return detail::cached_python_object(
        object_cache_,
        ask_price_attribute,
        [&] { return detail::double_object_new_ref(ask_price); });
  }

  nanobind::object ask_size_object() const {
    return detail::cached_python_object(
        object_cache_,
        ask_size_attribute,
        [&] { return detail::uint64_object_new_ref(ask_size); });
  }

  nanobind::object bid_exchange_object() const {
    return detail::cached_python_object(
        object_cache_,
        bid_exchange_attribute,
        [&] { return detail::uint64_object_new_ref(bid_exchange); });
  }

  nanobind::object bid_price_object() const {
    return detail::cached_python_object(
        object_cache_,
        bid_price_attribute,
        [&] { return detail::double_object_new_ref(bid_price); });
  }

  nanobind::object bid_size_object() const {
    return detail::cached_python_object(
        object_cache_,
        bid_size_attribute,
        [&] { return detail::uint64_object_new_ref(bid_size); });
  }

  nanobind::object conditions_object() const {
    return detail::cached_python_object(
        object_cache_,
        conditions_attribute,
        [&] {
          return detail::object_cache_new_ref(
              detail::bit_indices_frozenset(conditions));
        });
  }

  nanobind::object indicators_object() const {
    return detail::cached_python_object(
        object_cache_,
        indicators_attribute,
        [&] {
          return detail::object_cache_new_ref(
              detail::bit_indices_frozenset(indicators));
        });
  }

  nanobind::object participant_timestamp_object() const {
    return detail::cached_python_object(
        object_cache_,
        participant_timestamp_attribute,
        [&] { return detail::uint64_object_new_ref(participant_timestamp); });
  }

  nanobind::object sequence_number_object() const {
    return detail::cached_python_object(
        object_cache_,
        sequence_number_attribute,
        [&] { return detail::uint64_object_new_ref(sequence_number); });
  }

  nanobind::object sip_timestamp_object() const {
    return detail::cached_python_object(
        object_cache_,
        sip_timestamp_attribute,
        [&] { return detail::uint64_object_new_ref(sip_timestamp); });
  }

  nanobind::object tape_object() const {
    return detail::cached_python_object(
        object_cache_,
        tape_attribute,
        [&] { return detail::uint64_object_new_ref(tape); });
  }

  nanobind::object trf_timestamp_object() const {
    return detail::cached_python_object(
        object_cache_,
        trf_timestamp_attribute,
        [&] { return detail::uint64_object_new_ref(trf_timestamp); });
  }

  nanobind::list python_fields() const {
    nanobind::list values;
    values.append(ticker_object());
    values.append(ask_exchange_object());
    values.append(ask_price_object());
    values.append(ask_size_object());
    values.append(bid_exchange_object());
    values.append(bid_price_object());
    values.append(bid_size_object());
    values.append(conditions_object());
    values.append(indicators_object());
    values.append(participant_timestamp_object());
    values.append(sequence_number_object());
    values.append(sip_timestamp_object());
    values.append(tape_object());
    values.append(trf_timestamp_object());
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

struct CurrencyQuote {
  static constexpr std::size_t packed_size = 26;
  static constexpr std::size_t packed_ask_price_offset = 1;
  static constexpr std::size_t packed_bid_price_offset = 10;
  static constexpr std::size_t packed_participant_timestamp_offset = 18;
  using PackedData = detail::PackedBuffer<packed_size>;
  enum AttributeIndex : std::size_t {
    ticker_attribute,
    ask_exchange_attribute,
    ask_price_attribute,
    bid_exchange_attribute,
    bid_price_attribute,
    participant_timestamp_attribute,
    tickers_attribute,
    attribute_count,
  };

  std::string ticker;
  double ask_price = 0.0;
  double bid_price = 0.0;
  std::uint64_t participant_timestamp = 0;
  std::uint8_t ask_exchange = 0;
  std::uint8_t bid_exchange = 0;
  mutable std::unique_ptr<detail::LazyPythonObjectCache<attribute_count>> object_cache_;

  CurrencyQuote() = default;

  CurrencyQuote(const CurrencyQuote& other)
      : ticker(other.ticker),
        ask_price(other.ask_price),
        bid_price(other.bid_price),
        participant_timestamp(other.participant_timestamp),
        ask_exchange(other.ask_exchange),
        bid_exchange(other.bid_exchange) {}

  CurrencyQuote& operator=(const CurrencyQuote& other) {
    if (this == &other) {
      return *this;
    }

    ticker = other.ticker;
    ask_price = other.ask_price;
    bid_price = other.bid_price;
    participant_timestamp = other.participant_timestamp;
    ask_exchange = other.ask_exchange;
    bid_exchange = other.bid_exchange;
    object_cache_.reset();
    return *this;
  }

  CurrencyQuote(CurrencyQuote&&) noexcept = default;
  CurrencyQuote& operator=(CurrencyQuote&&) noexcept = default;

  CurrencyQuote(std::string_view packed_data, std::string_view ticker_value) {
    *this = from_packed(packed_data, ticker_value);
  }

  CurrencyQuote(const char* packed_data, std::string_view ticker_value)
      : CurrencyQuote(std::string_view(packed_data, packed_size), ticker_value) {}

  template <typename Specialization>
  static CurrencyQuote from_fields(const std::vector<std::string>& fields) {
    CurrencyQuote result;
    detail::require_field_count("CurrencyQuote", fields.size(), 6);
    result.ticker = fields[0];
    result.ask_exchange =
        Specialization::template parse_integer<std::uint8_t>(fields[1], "ask_exchange");
    result.ask_price = Specialization::parse_double(fields[2], "ask_price");
    result.bid_exchange =
        Specialization::template parse_integer<std::uint8_t>(fields[3], "bid_exchange");
    result.bid_price = Specialization::parse_double(fields[4], "bid_price");
    result.participant_timestamp = Specialization::template parse_integer<std::uint64_t>(
        fields[5],
        "participant_timestamp");
    return result;
  }

  static CurrencyQuote from_packed(std::string_view packed_data) {
    detail::require_packed_size("CurrencyQuote", packed_data.size(), packed_size);

    CurrencyQuote result;
    std::size_t offset = 0;
    result.ask_exchange = detail::read_unsigned_le<std::uint8_t>(packed_data, offset);
    result.ask_price = detail::read_double_le(packed_data, offset);
    result.bid_exchange = detail::read_unsigned_le<std::uint8_t>(packed_data, offset);
    result.bid_price = detail::read_double_le(packed_data, offset);
    result.participant_timestamp =
        detail::read_unsigned_le<std::uint64_t>(packed_data, offset);
    return result;
  }

  static CurrencyQuote from_packed(std::string_view packed_data, std::string_view ticker_value) {
    CurrencyQuote result = from_packed(packed_data);
    result.ticker.assign(ticker_value);
    return result;
  }

  static CurrencyQuote from_packed_data(const char* packed_data, std::string_view ticker_value) {
    return from_packed(std::string_view(packed_data, packed_size), ticker_value);
  }

  static std::uint64_t participant_timestamp_at(const void* packed_data) {
    return detail::read_uint64_le_at(
        packed_data,
        packed_participant_timestamp_offset);
  }

  static double ask_price_at(const void* packed_data) {
    return detail::read_double_le_at(packed_data, packed_ask_price_offset);
  }

  static double bid_price_at(const void* packed_data) {
    return detail::read_double_le_at(packed_data, packed_bid_price_offset);
  }

  PackedData pack() const {
    PackedData output{};
    std::size_t offset = 0;
    detail::write_unsigned_le(output, offset, ask_exchange);
    detail::write_double_le(output, offset, ask_price);
    detail::write_unsigned_le(output, offset, bid_exchange);
    detail::write_double_le(output, offset, bid_price);
    detail::write_unsigned_le(output, offset, participant_timestamp);
    return output;
  }

  nanobind::bytes packed_bytes() const {
    return detail::packed_bytes(pack());
  }

  bool operator==(const CurrencyQuote& other) const {
    return ticker == other.ticker &&
           ask_exchange == other.ask_exchange &&
           ask_price == other.ask_price &&
           bid_exchange == other.bid_exchange &&
           bid_price == other.bid_price &&
           participant_timestamp == other.participant_timestamp;
  }

  nanobind::object ticker_object() const {
    return detail::cached_python_object(
        object_cache_,
        ticker_attribute,
        [&] { return detail::string_object_new_ref(ticker); });
  }

  nanobind::object ask_exchange_object() const {
    return detail::cached_python_object(
        object_cache_,
        ask_exchange_attribute,
        [&] { return detail::uint64_object_new_ref(ask_exchange); });
  }

  nanobind::object ask_price_object() const {
    return detail::cached_python_object(
        object_cache_,
        ask_price_attribute,
        [&] { return detail::double_object_new_ref(ask_price); });
  }

  nanobind::object bid_exchange_object() const {
    return detail::cached_python_object(
        object_cache_,
        bid_exchange_attribute,
        [&] { return detail::uint64_object_new_ref(bid_exchange); });
  }

  nanobind::object bid_price_object() const {
    return detail::cached_python_object(
        object_cache_,
        bid_price_attribute,
        [&] { return detail::double_object_new_ref(bid_price); });
  }

  nanobind::object participant_timestamp_object() const {
    return detail::cached_python_object(
        object_cache_,
        participant_timestamp_attribute,
        [&] { return detail::uint64_object_new_ref(participant_timestamp); });
  }

  nanobind::list python_fields() const {
    nanobind::list values;
    values.append(ticker_object());
    values.append(ask_exchange_object());
    values.append(ask_price_object());
    values.append(bid_exchange_object());
    values.append(bid_price_object());
    values.append(participant_timestamp_object());
    return values;
  }

  nanobind::object tickers_object() const {
    return detail::cached_python_object(
        object_cache_,
        tickers_attribute,
        [&] {
          return detail::object_cache_new_ref(
              detail::currency_tickers_tuple(ticker));
        });
  }

  std::size_t hash_value() const {
    std::size_t seed = 0;
    detail::hash_combine(seed, ticker);
    detail::hash_combine(seed, ask_exchange);
    detail::hash_combine(seed, ask_price);
    detail::hash_combine(seed, bid_exchange);
    detail::hash_combine(seed, bid_price);
    detail::hash_combine(seed, participant_timestamp);
    return seed;
  }

  std::string repr() const {
    std::ostringstream out;
    out << "CurrencyQuote("
        << "ticker='" << ticker << "', "
        << "ask_exchange=" << static_cast<unsigned>(ask_exchange) << ", "
        << "ask_price=" << ask_price << ", "
        << "bid_exchange=" << static_cast<unsigned>(bid_exchange) << ", "
        << "bid_price=" << bid_price << ", "
        << "participant_timestamp=" << participant_timestamp << ")";
    return out.str();
  }
};

struct StockAggregate {
  static constexpr std::size_t packed_size = 56;
  using PackedData = detail::PackedBuffer<packed_size>;
  enum AttributeIndex : std::size_t {
    ticker_attribute,
    volume_attribute,
    open_attribute,
    close_attribute,
    high_attribute,
    low_attribute,
    window_start_attribute,
    transactions_attribute,
    attribute_count,
  };

  std::string ticker;
  double open = 0.0;
  double close = 0.0;
  double high = 0.0;
  double low = 0.0;
  std::uint64_t volume = 0;
  std::uint64_t window_start = 0;
  std::uint64_t transactions = 0;
  mutable std::unique_ptr<detail::LazyPythonObjectCache<attribute_count>> object_cache_;

  StockAggregate() = default;

  StockAggregate(const StockAggregate& other)
      : ticker(other.ticker),
        open(other.open),
        close(other.close),
        high(other.high),
        low(other.low),
        volume(other.volume),
        window_start(other.window_start),
        transactions(other.transactions) {}

  StockAggregate& operator=(const StockAggregate& other) {
    if (this == &other) {
      return *this;
    }

    ticker = other.ticker;
    open = other.open;
    close = other.close;
    high = other.high;
    low = other.low;
    volume = other.volume;
    window_start = other.window_start;
    transactions = other.transactions;
    object_cache_.reset();
    return *this;
  }

  StockAggregate(StockAggregate&&) noexcept = default;
  StockAggregate& operator=(StockAggregate&&) noexcept = default;

  StockAggregate(std::string_view packed_data, std::string_view ticker_value) {
    *this = from_packed(packed_data, ticker_value);
  }

  StockAggregate(const char* packed_data, std::string_view ticker_value)
      : StockAggregate(std::string_view(packed_data, packed_size), ticker_value) {}

  template <typename Specialization>
  static StockAggregate from_fields(const std::vector<std::string>& fields) {
    StockAggregate result;
    detail::require_field_count("StockAggregate", fields.size(), 8);
    result.ticker = fields[0];
    result.volume =
        Specialization::template parse_integer<std::uint64_t>(fields[1], "volume");
    result.open = Specialization::parse_double(fields[2], "open");
    result.close = Specialization::parse_double(fields[3], "close");
    result.high = Specialization::parse_double(fields[4], "high");
    result.low = Specialization::parse_double(fields[5], "low");
    result.window_start =
        Specialization::template parse_integer<std::uint64_t>(fields[6], "window_start");
    result.transactions =
        Specialization::template parse_integer<std::uint64_t>(fields[7], "transactions");
    return result;
  }

  static StockAggregate from_packed(std::string_view packed_data) {
    detail::require_packed_size("StockAggregate", packed_data.size(), packed_size);

    StockAggregate result;
    std::size_t offset = 0;
    result.volume = detail::read_unsigned_le<std::uint64_t>(packed_data, offset);
    result.open = detail::read_double_le(packed_data, offset);
    result.close = detail::read_double_le(packed_data, offset);
    result.high = detail::read_double_le(packed_data, offset);
    result.low = detail::read_double_le(packed_data, offset);
    result.window_start = detail::read_unsigned_le<std::uint64_t>(packed_data, offset);
    result.transactions = detail::read_unsigned_le<std::uint64_t>(packed_data, offset);
    return result;
  }

  static StockAggregate from_packed(
      std::string_view packed_data,
      std::string_view ticker_value) {
    StockAggregate result = from_packed(packed_data);
    result.ticker.assign(ticker_value);
    return result;
  }

  static StockAggregate from_packed_data(
      const char* packed_data,
      std::string_view ticker_value) {
    return from_packed(std::string_view(packed_data, packed_size), ticker_value);
  }

  PackedData pack() const {
    PackedData output{};
    std::size_t offset = 0;
    detail::write_unsigned_le(output, offset, volume);
    detail::write_double_le(output, offset, open);
    detail::write_double_le(output, offset, close);
    detail::write_double_le(output, offset, high);
    detail::write_double_le(output, offset, low);
    detail::write_unsigned_le(output, offset, window_start);
    detail::write_unsigned_le(output, offset, transactions);
    return output;
  }

  nanobind::bytes packed_bytes() const {
    return detail::packed_bytes(pack());
  }

  bool operator==(const StockAggregate& other) const {
    return ticker == other.ticker &&
           volume == other.volume &&
           open == other.open &&
           close == other.close &&
           high == other.high &&
           low == other.low &&
           window_start == other.window_start &&
           transactions == other.transactions;
  }

  nanobind::object ticker_object() const {
    return detail::cached_python_object(
        object_cache_,
        ticker_attribute,
        [&] { return detail::string_object_new_ref(ticker); });
  }

  nanobind::object volume_object() const {
    return detail::cached_python_object(
        object_cache_,
        volume_attribute,
        [&] { return detail::uint64_object_new_ref(volume); });
  }

  nanobind::object open_object() const {
    return detail::cached_python_object(
        object_cache_,
        open_attribute,
        [&] { return detail::double_object_new_ref(open); });
  }

  nanobind::object close_object() const {
    return detail::cached_python_object(
        object_cache_,
        close_attribute,
        [&] { return detail::double_object_new_ref(close); });
  }

  nanobind::object high_object() const {
    return detail::cached_python_object(
        object_cache_,
        high_attribute,
        [&] { return detail::double_object_new_ref(high); });
  }

  nanobind::object low_object() const {
    return detail::cached_python_object(
        object_cache_,
        low_attribute,
        [&] { return detail::double_object_new_ref(low); });
  }

  nanobind::object window_start_object() const {
    return detail::cached_python_object(
        object_cache_,
        window_start_attribute,
        [&] { return detail::uint64_object_new_ref(window_start); });
  }

  nanobind::object transactions_object() const {
    return detail::cached_python_object(
        object_cache_,
        transactions_attribute,
        [&] { return detail::uint64_object_new_ref(transactions); });
  }

  nanobind::list python_fields() const {
    nanobind::list values;
    values.append(ticker_object());
    values.append(volume_object());
    values.append(open_object());
    values.append(close_object());
    values.append(high_object());
    values.append(low_object());
    values.append(window_start_object());
    values.append(transactions_object());
    return values;
  }

  std::size_t hash_value() const {
    std::size_t seed = 0;
    detail::hash_combine(seed, ticker);
    detail::hash_combine(seed, volume);
    detail::hash_combine(seed, open);
    detail::hash_combine(seed, close);
    detail::hash_combine(seed, high);
    detail::hash_combine(seed, low);
    detail::hash_combine(seed, window_start);
    detail::hash_combine(seed, transactions);
    return seed;
  }

  std::string repr() const {
    std::ostringstream out;
    out << "StockAggregate("
        << "ticker='" << ticker << "', "
        << "volume=" << volume << ", "
        << "open=" << open << ", "
        << "close=" << close << ", "
        << "high=" << high << ", "
        << "low=" << low << ", "
        << "window_start=" << window_start << ", "
        << "transactions=" << transactions << ")";
    return out.str();
  }
};

struct CurrencyAggregate {
  static constexpr std::size_t packed_size = 56;
  using PackedData = detail::PackedBuffer<packed_size>;
  enum AttributeIndex : std::size_t {
    ticker_attribute,
    volume_attribute,
    open_attribute,
    close_attribute,
    high_attribute,
    low_attribute,
    window_start_attribute,
    transactions_attribute,
    tickers_attribute,
    attribute_count,
  };

  std::string ticker;
  double open = 0.0;
  double close = 0.0;
  double high = 0.0;
  double low = 0.0;
  std::uint64_t volume = 0;
  std::uint64_t window_start = 0;
  std::uint64_t transactions = 0;
  mutable std::unique_ptr<detail::LazyPythonObjectCache<attribute_count>> object_cache_;

  CurrencyAggregate() = default;

  CurrencyAggregate(const CurrencyAggregate& other)
      : ticker(other.ticker),
        open(other.open),
        close(other.close),
        high(other.high),
        low(other.low),
        volume(other.volume),
        window_start(other.window_start),
        transactions(other.transactions) {}

  CurrencyAggregate& operator=(const CurrencyAggregate& other) {
    if (this == &other) {
      return *this;
    }

    ticker = other.ticker;
    open = other.open;
    close = other.close;
    high = other.high;
    low = other.low;
    volume = other.volume;
    window_start = other.window_start;
    transactions = other.transactions;
    object_cache_.reset();
    return *this;
  }

  CurrencyAggregate(CurrencyAggregate&&) noexcept = default;
  CurrencyAggregate& operator=(CurrencyAggregate&&) noexcept = default;

  CurrencyAggregate(std::string_view packed_data, std::string_view ticker_value) {
    *this = from_packed(packed_data, ticker_value);
  }

  CurrencyAggregate(const char* packed_data, std::string_view ticker_value)
      : CurrencyAggregate(std::string_view(packed_data, packed_size), ticker_value) {}

  template <typename Specialization>
  static CurrencyAggregate from_fields(const std::vector<std::string>& fields) {
    CurrencyAggregate result;
    detail::require_field_count("CurrencyAggregate", fields.size(), 8);
    result.ticker = fields[0];
    result.volume =
        Specialization::template parse_integer<std::uint64_t>(fields[1], "volume");
    result.open = Specialization::parse_double(fields[2], "open");
    result.close = Specialization::parse_double(fields[3], "close");
    result.high = Specialization::parse_double(fields[4], "high");
    result.low = Specialization::parse_double(fields[5], "low");
    result.window_start =
        Specialization::template parse_integer<std::uint64_t>(fields[6], "window_start");
    result.transactions =
        Specialization::template parse_integer<std::uint64_t>(fields[7], "transactions");
    return result;
  }

  static CurrencyAggregate from_packed(std::string_view packed_data) {
    detail::require_packed_size("CurrencyAggregate", packed_data.size(), packed_size);

    CurrencyAggregate result;
    std::size_t offset = 0;
    result.volume = detail::read_unsigned_le<std::uint64_t>(packed_data, offset);
    result.open = detail::read_double_le(packed_data, offset);
    result.close = detail::read_double_le(packed_data, offset);
    result.high = detail::read_double_le(packed_data, offset);
    result.low = detail::read_double_le(packed_data, offset);
    result.window_start = detail::read_unsigned_le<std::uint64_t>(packed_data, offset);
    result.transactions = detail::read_unsigned_le<std::uint64_t>(packed_data, offset);
    return result;
  }

  static CurrencyAggregate from_packed(
      std::string_view packed_data,
      std::string_view ticker_value) {
    CurrencyAggregate result = from_packed(packed_data);
    result.ticker.assign(ticker_value);
    return result;
  }

  static CurrencyAggregate from_packed_data(
      const char* packed_data,
      std::string_view ticker_value) {
    return from_packed(std::string_view(packed_data, packed_size), ticker_value);
  }

  PackedData pack() const {
    PackedData output{};
    std::size_t offset = 0;
    detail::write_unsigned_le(output, offset, volume);
    detail::write_double_le(output, offset, open);
    detail::write_double_le(output, offset, close);
    detail::write_double_le(output, offset, high);
    detail::write_double_le(output, offset, low);
    detail::write_unsigned_le(output, offset, window_start);
    detail::write_unsigned_le(output, offset, transactions);
    return output;
  }

  nanobind::bytes packed_bytes() const {
    return detail::packed_bytes(pack());
  }

  bool operator==(const CurrencyAggregate& other) const {
    return ticker == other.ticker &&
           volume == other.volume &&
           open == other.open &&
           close == other.close &&
           high == other.high &&
           low == other.low &&
           window_start == other.window_start &&
           transactions == other.transactions;
  }

  nanobind::object ticker_object() const {
    return detail::cached_python_object(
        object_cache_,
        ticker_attribute,
        [&] { return detail::string_object_new_ref(ticker); });
  }

  nanobind::object volume_object() const {
    return detail::cached_python_object(
        object_cache_,
        volume_attribute,
        [&] { return detail::uint64_object_new_ref(volume); });
  }

  nanobind::object open_object() const {
    return detail::cached_python_object(
        object_cache_,
        open_attribute,
        [&] { return detail::double_object_new_ref(open); });
  }

  nanobind::object close_object() const {
    return detail::cached_python_object(
        object_cache_,
        close_attribute,
        [&] { return detail::double_object_new_ref(close); });
  }

  nanobind::object high_object() const {
    return detail::cached_python_object(
        object_cache_,
        high_attribute,
        [&] { return detail::double_object_new_ref(high); });
  }

  nanobind::object low_object() const {
    return detail::cached_python_object(
        object_cache_,
        low_attribute,
        [&] { return detail::double_object_new_ref(low); });
  }

  nanobind::object window_start_object() const {
    return detail::cached_python_object(
        object_cache_,
        window_start_attribute,
        [&] { return detail::uint64_object_new_ref(window_start); });
  }

  nanobind::object transactions_object() const {
    return detail::cached_python_object(
        object_cache_,
        transactions_attribute,
        [&] { return detail::uint64_object_new_ref(transactions); });
  }

  nanobind::list python_fields() const {
    nanobind::list values;
    values.append(ticker_object());
    values.append(volume_object());
    values.append(open_object());
    values.append(close_object());
    values.append(high_object());
    values.append(low_object());
    values.append(window_start_object());
    values.append(transactions_object());
    return values;
  }

  nanobind::object tickers_object() const {
    return detail::cached_python_object(
        object_cache_,
        tickers_attribute,
        [&] {
          return detail::object_cache_new_ref(
              detail::currency_tickers_tuple(ticker));
        });
  }

  std::size_t hash_value() const {
    std::size_t seed = 0;
    detail::hash_combine(seed, ticker);
    detail::hash_combine(seed, volume);
    detail::hash_combine(seed, open);
    detail::hash_combine(seed, close);
    detail::hash_combine(seed, high);
    detail::hash_combine(seed, low);
    detail::hash_combine(seed, window_start);
    detail::hash_combine(seed, transactions);
    return seed;
  }

  std::string repr() const {
    std::ostringstream out;
    out << "CurrencyAggregate("
        << "ticker='" << ticker << "', "
        << "volume=" << volume << ", "
        << "open=" << open << ", "
        << "close=" << close << ", "
        << "high=" << high << ", "
        << "low=" << low << ", "
        << "window_start=" << window_start << ", "
        << "transactions=" << transactions << ")";
    return out.str();
  }
};

namespace detail {

inline double quiet_nan() {
  return std::numeric_limits<double>::quiet_NaN();
}

struct PriceAggregation {
  bool has_value = false;
  double open = 0.0;
  double close = 0.0;
  double high = 0.0;
  double low = 0.0;
  double mean = 0.0;
  double m2 = 0.0;
  std::uint64_t count = 0;

  void add(double value) {
    if (!has_value) {
      has_value = true;
      open = value;
      high = value;
      low = value;
    } else {
      high = std::max(high, value);
      low = std::min(low, value);
    }

    close = value;
    ++count;

    const double delta = value - mean;
    mean += delta / static_cast<double>(count);
    const double delta2 = value - mean;
    m2 += delta * delta2;
  }

  double average() const {
    return count == 0 ? quiet_nan() : mean;
  }

  double stddev() const {
    return count == 0 ? quiet_nan() : std::sqrt(m2 / static_cast<double>(count));
  }

  double change() const {
    return count == 0 ? quiet_nan() : close - open;
  }

  double range() const {
    return count == 0 ? quiet_nan() : high - low;
  }

  double return_bps() const {
    if (count == 0 || open == 0.0) {
      return quiet_nan();
    }
    return ((close / open) - 1.0) * 10'000.0;
  }

  double range_bps() const {
    if (count == 0 || open == 0.0) {
      return quiet_nan();
    }
    return ((high - low) / open) * 10'000.0;
  }
};

struct WeightedPriceAggregation {
  long double weighted_sum = 0.0;
  std::uint64_t weight = 0;

  void add(double value, std::uint64_t value_weight) {
    weighted_sum += static_cast<long double>(value) *
                    static_cast<long double>(value_weight);
    weight += value_weight;
  }

  double average() const {
    if (weight == 0) {
      return quiet_nan();
    }
    return static_cast<double>(
        weighted_sum / static_cast<long double>(weight));
  }
};

struct TimeWeightedPriceAggregation {
  bool has_previous = false;
  std::uint64_t previous_timestamp = 0;
  double previous_value = 0.0;
  long double weighted_sum = 0.0;
  std::uint64_t weight_ns = 0;

  void add(std::uint64_t timestamp, double value) {
    if (has_previous && timestamp > previous_timestamp) {
      const std::uint64_t delta = timestamp - previous_timestamp;
      weighted_sum += static_cast<long double>(previous_value) *
                      static_cast<long double>(delta);
      weight_ns += delta;
    }

    has_previous = true;
    previous_timestamp = timestamp;
    previous_value = value;
  }

  double average_until(std::uint64_t end_timestamp) const {
    long double total = weighted_sum;
    std::uint64_t total_weight = weight_ns;
    if (has_previous && end_timestamp > previous_timestamp) {
      const std::uint64_t delta = end_timestamp - previous_timestamp;
      total += static_cast<long double>(previous_value) *
               static_cast<long double>(delta);
      total_weight += delta;
    }

    if (total_weight == 0) {
      return quiet_nan();
    }
    return static_cast<double>(total / static_cast<long double>(total_weight));
  }
};

inline std::uint64_t saturating_add_uint64(
    std::uint64_t left,
    std::uint64_t right) {
  if (std::numeric_limits<std::uint64_t>::max() - left < right) {
    return std::numeric_limits<std::uint64_t>::max();
  }
  return left + right;
}

inline std::uint64_t seconds_to_ns(
    std::uint64_t seconds,
    std::string_view name) {
  constexpr std::uint64_t nanoseconds_per_second = 1'000'000'000ULL;
  if (seconds > std::numeric_limits<std::uint64_t>::max() / nanoseconds_per_second) {
    std::ostringstream message;
    message << name << " is too large to convert to nanoseconds";
    throw std::invalid_argument(message.str());
  }
  return seconds * nanoseconds_per_second;
}

inline std::uint64_t aggregation_window_start(
    std::uint64_t timestamp,
    std::uint64_t interval_ns,
    std::uint64_t offset_ns) {
  if (timestamp < offset_ns) {
    return 0;
  }

  return ((timestamp - offset_ns) / interval_ns) * interval_ns + offset_ns;
}

}  // namespace detail

struct StockTradeAggregation : detail::AggregateObjectCache<22> {
  enum AttributeIndex : std::size_t {
    ticker_attribute,
    open_attribute,
    close_attribute,
    high_attribute,
    low_attribute,
    avg_attribute,
    volume_weighted_avg_attribute,
    volume_attribute,
    window_start_attribute,
    transactions_attribute,
    stddev_attribute,
    dollar_volume_attribute,
    avg_trade_size_attribute,
    min_trade_size_attribute,
    max_trade_size_attribute,
    price_change_attribute,
    return_bps_attribute,
    price_range_attribute,
    range_bps_attribute,
    first_timestamp_attribute,
    last_timestamp_attribute,
    duration_ns_attribute,
  };

  std::string ticker;
  double open = 0.0;
  double close = 0.0;
  double high = 0.0;
  double low = 0.0;
  double avg = 0.0;
  double volume_weighted_avg = 0.0;
  double stddev = 0.0;
  double dollar_volume = 0.0;
  double avg_trade_size = 0.0;
  double price_change = 0.0;
  double return_bps = 0.0;
  double price_range = 0.0;
  double range_bps = 0.0;
  std::uint64_t volume = 0;
  std::uint64_t window_start = 0;
  std::uint64_t transactions = 0;
  std::uint64_t min_trade_size = 0;
  std::uint64_t max_trade_size = 0;
  std::uint64_t first_timestamp = 0;
  std::uint64_t last_timestamp = 0;
  std::uint64_t duration_ns = 0;
};

struct StockQuoteAggregation : detail::AggregateObjectCache<60> {
  enum AttributeIndex : std::size_t {
    ticker_attribute,
    ask_open_attribute,
    ask_close_attribute,
    ask_high_attribute,
    ask_low_attribute,
    ask_avg_attribute,
    ask_volume_weighted_avg_attribute,
    ask_volume_attribute,
    ask_stddev_attribute,
    bid_open_attribute,
    bid_close_attribute,
    bid_high_attribute,
    bid_low_attribute,
    bid_avg_attribute,
    bid_volume_weighted_avg_attribute,
    bid_volume_attribute,
    bid_stddev_attribute,
    window_start_attribute,
    transactions_attribute,
    ask_change_attribute,
    ask_return_bps_attribute,
    ask_range_attribute,
    ask_range_bps_attribute,
    bid_change_attribute,
    bid_return_bps_attribute,
    bid_range_attribute,
    bid_range_bps_attribute,
    spread_open_attribute,
    spread_close_attribute,
    spread_high_attribute,
    spread_low_attribute,
    spread_avg_attribute,
    spread_stddev_attribute,
    spread_change_attribute,
    spread_return_bps_attribute,
    spread_range_attribute,
    spread_range_bps_attribute,
    mid_open_attribute,
    mid_close_attribute,
    mid_high_attribute,
    mid_low_attribute,
    mid_avg_attribute,
    mid_stddev_attribute,
    mid_change_attribute,
    mid_return_bps_attribute,
    mid_range_attribute,
    mid_range_bps_attribute,
    locked_count_attribute,
    crossed_count_attribute,
    zero_ask_size_count_attribute,
    zero_bid_size_count_attribute,
    size_imbalance_avg_attribute,
    microprice_avg_attribute,
    time_weighted_ask_avg_attribute,
    time_weighted_bid_avg_attribute,
    time_weighted_mid_avg_attribute,
    time_weighted_spread_avg_attribute,
    first_timestamp_attribute,
    last_timestamp_attribute,
    duration_ns_attribute,
  };

  std::string ticker;
  double ask_open = 0.0;
  double ask_close = 0.0;
  double ask_high = 0.0;
  double ask_low = 0.0;
  double ask_avg = 0.0;
  double ask_volume_weighted_avg = 0.0;
  double ask_stddev = 0.0;
  double bid_open = 0.0;
  double bid_close = 0.0;
  double bid_high = 0.0;
  double bid_low = 0.0;
  double bid_avg = 0.0;
  double bid_volume_weighted_avg = 0.0;
  double bid_stddev = 0.0;
  double ask_change = 0.0;
  double ask_return_bps = 0.0;
  double ask_range = 0.0;
  double ask_range_bps = 0.0;
  double bid_change = 0.0;
  double bid_return_bps = 0.0;
  double bid_range = 0.0;
  double bid_range_bps = 0.0;
  double spread_open = 0.0;
  double spread_close = 0.0;
  double spread_high = 0.0;
  double spread_low = 0.0;
  double spread_avg = 0.0;
  double spread_stddev = 0.0;
  double spread_change = 0.0;
  double spread_return_bps = 0.0;
  double spread_range = 0.0;
  double spread_range_bps = 0.0;
  double mid_open = 0.0;
  double mid_close = 0.0;
  double mid_high = 0.0;
  double mid_low = 0.0;
  double mid_avg = 0.0;
  double mid_stddev = 0.0;
  double mid_change = 0.0;
  double mid_return_bps = 0.0;
  double mid_range = 0.0;
  double mid_range_bps = 0.0;
  double size_imbalance_avg = 0.0;
  double microprice_avg = 0.0;
  double time_weighted_ask_avg = 0.0;
  double time_weighted_bid_avg = 0.0;
  double time_weighted_mid_avg = 0.0;
  double time_weighted_spread_avg = 0.0;
  std::uint64_t ask_volume = 0;
  std::uint64_t bid_volume = 0;
  std::uint64_t window_start = 0;
  std::uint64_t transactions = 0;
  std::uint64_t locked_count = 0;
  std::uint64_t crossed_count = 0;
  std::uint64_t zero_ask_size_count = 0;
  std::uint64_t zero_bid_size_count = 0;
  std::uint64_t first_timestamp = 0;
  std::uint64_t last_timestamp = 0;
  std::uint64_t duration_ns = 0;
};

struct CurrencyQuoteAggregation : detail::AggregateObjectCache<52> {
  enum AttributeIndex : std::size_t {
    ticker_attribute,
    ask_open_attribute,
    ask_close_attribute,
    ask_high_attribute,
    ask_low_attribute,
    ask_avg_attribute,
    ask_stddev_attribute,
    bid_open_attribute,
    bid_close_attribute,
    bid_high_attribute,
    bid_low_attribute,
    bid_avg_attribute,
    bid_stddev_attribute,
    window_start_attribute,
    transactions_attribute,
    ask_change_attribute,
    ask_return_bps_attribute,
    ask_range_attribute,
    ask_range_bps_attribute,
    bid_change_attribute,
    bid_return_bps_attribute,
    bid_range_attribute,
    bid_range_bps_attribute,
    spread_open_attribute,
    spread_close_attribute,
    spread_high_attribute,
    spread_low_attribute,
    spread_avg_attribute,
    spread_stddev_attribute,
    spread_change_attribute,
    spread_return_bps_attribute,
    spread_range_attribute,
    spread_range_bps_attribute,
    mid_open_attribute,
    mid_close_attribute,
    mid_high_attribute,
    mid_low_attribute,
    mid_avg_attribute,
    mid_stddev_attribute,
    mid_change_attribute,
    mid_return_bps_attribute,
    mid_range_attribute,
    mid_range_bps_attribute,
    locked_count_attribute,
    crossed_count_attribute,
    time_weighted_ask_avg_attribute,
    time_weighted_bid_avg_attribute,
    time_weighted_mid_avg_attribute,
    time_weighted_spread_avg_attribute,
    first_timestamp_attribute,
    last_timestamp_attribute,
    duration_ns_attribute,
  };

  std::string ticker;
  double ask_open = 0.0;
  double ask_close = 0.0;
  double ask_high = 0.0;
  double ask_low = 0.0;
  double ask_avg = 0.0;
  double ask_stddev = 0.0;
  double bid_open = 0.0;
  double bid_close = 0.0;
  double bid_high = 0.0;
  double bid_low = 0.0;
  double bid_avg = 0.0;
  double bid_stddev = 0.0;
  double ask_change = 0.0;
  double ask_return_bps = 0.0;
  double ask_range = 0.0;
  double ask_range_bps = 0.0;
  double bid_change = 0.0;
  double bid_return_bps = 0.0;
  double bid_range = 0.0;
  double bid_range_bps = 0.0;
  double spread_open = 0.0;
  double spread_close = 0.0;
  double spread_high = 0.0;
  double spread_low = 0.0;
  double spread_avg = 0.0;
  double spread_stddev = 0.0;
  double spread_change = 0.0;
  double spread_return_bps = 0.0;
  double spread_range = 0.0;
  double spread_range_bps = 0.0;
  double mid_open = 0.0;
  double mid_close = 0.0;
  double mid_high = 0.0;
  double mid_low = 0.0;
  double mid_avg = 0.0;
  double mid_stddev = 0.0;
  double mid_change = 0.0;
  double mid_return_bps = 0.0;
  double mid_range = 0.0;
  double mid_range_bps = 0.0;
  double time_weighted_ask_avg = 0.0;
  double time_weighted_bid_avg = 0.0;
  double time_weighted_mid_avg = 0.0;
  double time_weighted_spread_avg = 0.0;
  std::uint64_t window_start = 0;
  std::uint64_t transactions = 0;
  std::uint64_t locked_count = 0;
  std::uint64_t crossed_count = 0;
  std::uint64_t first_timestamp = 0;
  std::uint64_t last_timestamp = 0;
  std::uint64_t duration_ns = 0;
};

struct StockTradeAggregationState {
  std::string ticker;
  std::uint64_t window_start = 0;
  std::uint64_t window_end = 0;
  std::uint64_t transactions = 0;
  std::uint64_t volume = 0;
  std::uint64_t min_trade_size = 0;
  std::uint64_t max_trade_size = 0;
  std::uint64_t first_timestamp = 0;
  std::uint64_t last_timestamp = 0;
  detail::PriceAggregation price;
  detail::WeightedPriceAggregation weighted_price;

  StockTradeAggregationState() = default;

  StockTradeAggregationState(
      std::string ticker_value,
      std::uint64_t window,
      std::uint64_t interval_ns = 0)
      : ticker(std::move(ticker_value)),
        window_start(window),
        window_end(detail::saturating_add_uint64(window, interval_ns)) {}

  void add(const StockTrade& row) {
    const std::uint64_t row_volume =
        row.size <= 0 ? 0 : static_cast<std::uint64_t>(row.size);
    add_values(row.price, row_volume, row.sip_timestamp);
  }

  void add_values(
      double value,
      std::uint64_t row_volume,
      std::uint64_t timestamp) {
    price.add(value);
    weighted_price.add(value, row_volume);
    volume += row_volume;
    if (transactions == 0) {
      min_trade_size = row_volume;
      max_trade_size = row_volume;
      first_timestamp = timestamp;
    } else {
      min_trade_size = std::min(min_trade_size, row_volume);
      max_trade_size = std::max(max_trade_size, row_volume);
    }
    last_timestamp = timestamp;
    ++transactions;
  }

  StockTradeAggregation to_result() const {
    StockTradeAggregation result;
    result.ticker = ticker;
    result.open = price.open;
    result.close = price.close;
    result.high = price.high;
    result.low = price.low;
    result.avg = price.average();
    result.volume_weighted_avg = weighted_price.average();
    result.volume = volume;
    result.window_start = window_start;
    result.transactions = transactions;
    result.stddev = price.stddev();
    result.dollar_volume = static_cast<double>(
        weighted_price.weight == 0 ? 0.0L : weighted_price.weighted_sum);
    result.avg_trade_size =
        transactions == 0
            ? detail::quiet_nan()
            : static_cast<double>(volume) / static_cast<double>(transactions);
    result.min_trade_size = min_trade_size;
    result.max_trade_size = max_trade_size;
    result.price_change = price.change();
    result.return_bps = price.return_bps();
    result.price_range = price.range();
    result.range_bps = price.range_bps();
    result.first_timestamp = first_timestamp;
    result.last_timestamp = last_timestamp;
    result.duration_ns =
        last_timestamp >= first_timestamp ? last_timestamp - first_timestamp : 0;
    return result;
  }
};

struct StockQuoteAggregationState {
  std::string ticker;
  std::uint64_t window_start = 0;
  std::uint64_t window_end = 0;
  std::uint64_t transactions = 0;
  std::uint64_t ask_volume = 0;
  std::uint64_t bid_volume = 0;
  std::uint64_t locked_count = 0;
  std::uint64_t crossed_count = 0;
  std::uint64_t zero_ask_size_count = 0;
  std::uint64_t zero_bid_size_count = 0;
  std::uint64_t first_timestamp = 0;
  std::uint64_t last_timestamp = 0;
  detail::PriceAggregation ask_price;
  detail::PriceAggregation bid_price;
  detail::PriceAggregation spread;
  detail::PriceAggregation mid;
  detail::PriceAggregation size_imbalance;
  detail::PriceAggregation microprice;
  detail::WeightedPriceAggregation weighted_ask_price;
  detail::WeightedPriceAggregation weighted_bid_price;
  detail::TimeWeightedPriceAggregation time_weighted_ask_price;
  detail::TimeWeightedPriceAggregation time_weighted_bid_price;
  detail::TimeWeightedPriceAggregation time_weighted_spread;
  detail::TimeWeightedPriceAggregation time_weighted_mid;

  StockQuoteAggregationState() = default;

  StockQuoteAggregationState(
      std::string ticker_value,
      std::uint64_t window,
      std::uint64_t interval_ns = 0)
      : ticker(std::move(ticker_value)),
        window_start(window),
        window_end(detail::saturating_add_uint64(window, interval_ns)) {}

  void add(const StockQuote& row) {
    add_values(
        row.ask_price,
        row.ask_size,
        row.bid_price,
        row.bid_size,
        row.sip_timestamp);
  }

  void add_values(
      double ask_value,
      std::uint64_t ask_size,
      double bid_value,
      std::uint64_t bid_size,
      std::uint64_t timestamp) {
    const double spread_value = ask_value - bid_value;
    const double mid_value = (ask_value + bid_value) * 0.5;
    const std::uint64_t combined_size = ask_size + bid_size;

    ask_price.add(ask_value);
    bid_price.add(bid_value);
    spread.add(spread_value);
    mid.add(mid_value);
    weighted_ask_price.add(ask_value, ask_size);
    weighted_bid_price.add(bid_value, bid_size);
    time_weighted_ask_price.add(timestamp, ask_value);
    time_weighted_bid_price.add(timestamp, bid_value);
    time_weighted_spread.add(timestamp, spread_value);
    time_weighted_mid.add(timestamp, mid_value);
    ask_volume += ask_size;
    bid_volume += bid_size;
    locked_count += ask_value == bid_value ? 1 : 0;
    crossed_count += bid_value > ask_value ? 1 : 0;
    zero_ask_size_count += ask_size == 0 ? 1 : 0;
    zero_bid_size_count += bid_size == 0 ? 1 : 0;
    if (combined_size != 0) {
      size_imbalance.add(
          (static_cast<double>(bid_size) - static_cast<double>(ask_size)) /
          static_cast<double>(combined_size));
      microprice.add(
          ((ask_value * static_cast<double>(bid_size)) +
           (bid_value * static_cast<double>(ask_size))) /
          static_cast<double>(combined_size));
    }
    if (transactions == 0) {
      first_timestamp = timestamp;
    }
    last_timestamp = timestamp;
    ++transactions;
  }

  StockQuoteAggregation to_result() const {
    StockQuoteAggregation result;
    result.ticker = ticker;
    result.ask_open = ask_price.open;
    result.ask_close = ask_price.close;
    result.ask_high = ask_price.high;
    result.ask_low = ask_price.low;
    result.ask_avg = ask_price.average();
    result.ask_volume_weighted_avg = weighted_ask_price.average();
    result.ask_volume = ask_volume;
    result.ask_stddev = ask_price.stddev();
    result.bid_open = bid_price.open;
    result.bid_close = bid_price.close;
    result.bid_high = bid_price.high;
    result.bid_low = bid_price.low;
    result.bid_avg = bid_price.average();
    result.bid_volume_weighted_avg = weighted_bid_price.average();
    result.bid_volume = bid_volume;
    result.bid_stddev = bid_price.stddev();
    result.window_start = window_start;
    result.transactions = transactions;
    result.ask_change = ask_price.change();
    result.ask_return_bps = ask_price.return_bps();
    result.ask_range = ask_price.range();
    result.ask_range_bps = ask_price.range_bps();
    result.bid_change = bid_price.change();
    result.bid_return_bps = bid_price.return_bps();
    result.bid_range = bid_price.range();
    result.bid_range_bps = bid_price.range_bps();
    result.spread_open = spread.open;
    result.spread_close = spread.close;
    result.spread_high = spread.high;
    result.spread_low = spread.low;
    result.spread_avg = spread.average();
    result.spread_stddev = spread.stddev();
    result.spread_change = spread.change();
    result.spread_return_bps = spread.return_bps();
    result.spread_range = spread.range();
    result.spread_range_bps = spread.range_bps();
    result.mid_open = mid.open;
    result.mid_close = mid.close;
    result.mid_high = mid.high;
    result.mid_low = mid.low;
    result.mid_avg = mid.average();
    result.mid_stddev = mid.stddev();
    result.mid_change = mid.change();
    result.mid_return_bps = mid.return_bps();
    result.mid_range = mid.range();
    result.mid_range_bps = mid.range_bps();
    result.locked_count = locked_count;
    result.crossed_count = crossed_count;
    result.zero_ask_size_count = zero_ask_size_count;
    result.zero_bid_size_count = zero_bid_size_count;
    result.size_imbalance_avg = size_imbalance.average();
    result.microprice_avg = microprice.average();
    result.time_weighted_ask_avg =
        time_weighted_ask_price.average_until(window_end);
    result.time_weighted_bid_avg =
        time_weighted_bid_price.average_until(window_end);
    result.time_weighted_mid_avg = time_weighted_mid.average_until(window_end);
    result.time_weighted_spread_avg =
        time_weighted_spread.average_until(window_end);
    result.first_timestamp = first_timestamp;
    result.last_timestamp = last_timestamp;
    result.duration_ns =
        last_timestamp >= first_timestamp ? last_timestamp - first_timestamp : 0;
    return result;
  }
};

struct CurrencyQuoteAggregationState {
  std::string ticker;
  std::uint64_t window_start = 0;
  std::uint64_t window_end = 0;
  std::uint64_t transactions = 0;
  std::uint64_t locked_count = 0;
  std::uint64_t crossed_count = 0;
  std::uint64_t first_timestamp = 0;
  std::uint64_t last_timestamp = 0;
  detail::PriceAggregation ask_price;
  detail::PriceAggregation bid_price;
  detail::PriceAggregation spread;
  detail::PriceAggregation mid;
  detail::TimeWeightedPriceAggregation time_weighted_ask_price;
  detail::TimeWeightedPriceAggregation time_weighted_bid_price;
  detail::TimeWeightedPriceAggregation time_weighted_spread;
  detail::TimeWeightedPriceAggregation time_weighted_mid;

  CurrencyQuoteAggregationState() = default;

  CurrencyQuoteAggregationState(
      std::string ticker_value,
      std::uint64_t window,
      std::uint64_t interval_ns = 0)
      : ticker(std::move(ticker_value)),
        window_start(window),
        window_end(detail::saturating_add_uint64(window, interval_ns)) {}

  void add(const CurrencyQuote& row) {
    add_values(row.ask_price, row.bid_price, row.participant_timestamp);
  }

  void add_values(
      double ask_value,
      double bid_value,
      std::uint64_t timestamp) {
    const double spread_value = ask_value - bid_value;
    const double mid_value = (ask_value + bid_value) * 0.5;

    ask_price.add(ask_value);
    bid_price.add(bid_value);
    spread.add(spread_value);
    mid.add(mid_value);
    time_weighted_ask_price.add(timestamp, ask_value);
    time_weighted_bid_price.add(timestamp, bid_value);
    time_weighted_spread.add(timestamp, spread_value);
    time_weighted_mid.add(timestamp, mid_value);
    locked_count += ask_value == bid_value ? 1 : 0;
    crossed_count += bid_value > ask_value ? 1 : 0;
    if (transactions == 0) {
      first_timestamp = timestamp;
    }
    last_timestamp = timestamp;
    ++transactions;
  }

  CurrencyQuoteAggregation to_result() const {
    CurrencyQuoteAggregation result;
    result.ticker = ticker;
    result.ask_open = ask_price.open;
    result.ask_close = ask_price.close;
    result.ask_high = ask_price.high;
    result.ask_low = ask_price.low;
    result.ask_avg = ask_price.average();
    result.ask_stddev = ask_price.stddev();
    result.bid_open = bid_price.open;
    result.bid_close = bid_price.close;
    result.bid_high = bid_price.high;
    result.bid_low = bid_price.low;
    result.bid_avg = bid_price.average();
    result.bid_stddev = bid_price.stddev();
    result.window_start = window_start;
    result.transactions = transactions;
    result.ask_change = ask_price.change();
    result.ask_return_bps = ask_price.return_bps();
    result.ask_range = ask_price.range();
    result.ask_range_bps = ask_price.range_bps();
    result.bid_change = bid_price.change();
    result.bid_return_bps = bid_price.return_bps();
    result.bid_range = bid_price.range();
    result.bid_range_bps = bid_price.range_bps();
    result.spread_open = spread.open;
    result.spread_close = spread.close;
    result.spread_high = spread.high;
    result.spread_low = spread.low;
    result.spread_avg = spread.average();
    result.spread_stddev = spread.stddev();
    result.spread_change = spread.change();
    result.spread_return_bps = spread.return_bps();
    result.spread_range = spread.range();
    result.spread_range_bps = spread.range_bps();
    result.mid_open = mid.open;
    result.mid_close = mid.close;
    result.mid_high = mid.high;
    result.mid_low = mid.low;
    result.mid_avg = mid.average();
    result.mid_stddev = mid.stddev();
    result.mid_change = mid.change();
    result.mid_return_bps = mid.return_bps();
    result.mid_range = mid.range();
    result.mid_range_bps = mid.range_bps();
    result.locked_count = locked_count;
    result.crossed_count = crossed_count;
    result.time_weighted_ask_avg =
        time_weighted_ask_price.average_until(window_end);
    result.time_weighted_bid_avg =
        time_weighted_bid_price.average_until(window_end);
    result.time_weighted_mid_avg = time_weighted_mid.average_until(window_end);
    result.time_weighted_spread_avg =
        time_weighted_spread.average_until(window_end);
    result.first_timestamp = first_timestamp;
    result.last_timestamp = last_timestamp;
    result.duration_ns =
        last_timestamp >= first_timestamp ? last_timestamp - first_timestamp : 0;
    return result;
  }
};

class StockTradeDatabase;
class StockQuoteDatabase;
class CurrencyQuoteDatabase;

struct StockTradeAggregationTraits {
  using RowType = StockTrade;
  using State = StockTradeAggregationState;
  using OutputType = StockTradeAggregation;
  using DatabaseType = StockTradeDatabase;

  static std::uint64_t timestamp(const RowType& row) {
    return row.sip_timestamp;
  }

  static std::uint64_t packed_timestamp(const void* packed_data) {
    return RowType::sip_timestamp_at(packed_data);
  }

  static void add_packed(
      State& state,
      const void* packed_data,
      std::uint64_t timestamp) {
    const std::int32_t size = RowType::size_at(packed_data);
    state.add_values(
        RowType::price_at(packed_data),
        size <= 0 ? 0 : static_cast<std::uint64_t>(size),
        timestamp);
  }
};

struct StockQuoteAggregationTraits {
  using RowType = StockQuote;
  using State = StockQuoteAggregationState;
  using OutputType = StockQuoteAggregation;
  using DatabaseType = StockQuoteDatabase;

  static std::uint64_t timestamp(const RowType& row) {
    return row.sip_timestamp;
  }

  static std::uint64_t packed_timestamp(const void* packed_data) {
    return RowType::sip_timestamp_at(packed_data);
  }

  static void add_packed(
      State& state,
      const void* packed_data,
      std::uint64_t timestamp) {
    state.add_values(
        RowType::ask_price_at(packed_data),
        RowType::ask_size_at(packed_data),
        RowType::bid_price_at(packed_data),
        RowType::bid_size_at(packed_data),
        timestamp);
  }
};

struct CurrencyQuoteAggregationTraits {
  using RowType = CurrencyQuote;
  using State = CurrencyQuoteAggregationState;
  using OutputType = CurrencyQuoteAggregation;
  using DatabaseType = CurrencyQuoteDatabase;

  static std::uint64_t timestamp(const RowType& row) {
    return row.participant_timestamp;
  }

  static std::uint64_t packed_timestamp(const void* packed_data) {
    return RowType::participant_timestamp_at(packed_data);
  }

  static void add_packed(
      State& state,
      const void* packed_data,
      std::uint64_t timestamp) {
    state.add_values(
        RowType::ask_price_at(packed_data),
        RowType::bid_price_at(packed_data),
        timestamp);
  }
};

template <typename Traits>
class WindowAggregator {
 public:
  using RowType = typename Traits::RowType;
  using State = typename Traits::State;
  using OutputType = typename Traits::OutputType;
  using DatabaseType = typename Traits::DatabaseType;

  WindowAggregator(
      nanobind::handle rows,
      std::uint64_t interval_seconds,
      std::uint64_t offset_seconds)
      : interval_ns_(detail::seconds_to_ns(interval_seconds, "interval_seconds")),
        offset_ns_(detail::seconds_to_ns(offset_seconds, "offset_seconds")) {
    if (interval_seconds == 0) {
      throw std::invalid_argument("interval_seconds must be greater than zero");
    }

    DatabaseType* database = nullptr;
    if (nanobind::try_cast<DatabaseType*>(rows, database, false)) {
      database_ = database;
      database_owner_ = nanobind::borrow<nanobind::object>(rows);
      return;
    }

    PyObject* iterator = PyObject_GetIter(rows.ptr());
    if (iterator == nullptr) {
      throw nanobind::python_error();
    }
    iterator_ = nanobind::steal<nanobind::object>(iterator);
  }

  WindowAggregator& iter() {
    return *this;
  }

  OutputType next() {
    if (database_ != nullptr) {
      return next_database();
    }

    RowType row;
    if (has_pending_row_) {
      row = std::move(pending_row_);
      has_pending_row_ = false;
    } else if (!read_next_row(row)) {
      throw nanobind::stop_iteration();
    }

    const std::uint64_t row_window_start =
        detail::aggregation_window_start(
            Traits::timestamp(row),
            interval_ns_,
            offset_ns_);
    State state(row.ticker, row_window_start, interval_ns_);
    state.add(row);

    while (read_next_row(row)) {
      const std::uint64_t next_window_start =
          detail::aggregation_window_start(
              Traits::timestamp(row),
              interval_ns_,
              offset_ns_);
      if (row.ticker == state.ticker && next_window_start == state.window_start) {
        state.add(row);
        continue;
      }

      pending_row_ = std::move(row);
      has_pending_row_ = true;
      break;
    }

    return state.to_result();
  }

 private:
  OutputType next_database() {
    if (database_index_ >= database_->size()) {
      throw nanobind::stop_iteration();
    }

    const void* packed_data = database_->packed_data_at(database_index_++);
    const std::uint64_t timestamp = Traits::packed_timestamp(packed_data);
    const std::uint64_t row_window_start =
        detail::aggregation_window_start(
            timestamp,
            interval_ns_,
            offset_ns_);

    State state(database_->ticker(), row_window_start, interval_ns_);
    Traits::add_packed(state, packed_data, timestamp);

    while (database_index_ < database_->size()) {
      packed_data = database_->packed_data_at(database_index_);
      const std::uint64_t next_timestamp = Traits::packed_timestamp(packed_data);
      const std::uint64_t next_window_start =
          detail::aggregation_window_start(
              next_timestamp,
              interval_ns_,
              offset_ns_);
      if (next_window_start != state.window_start) {
        break;
      }

      Traits::add_packed(state, packed_data, next_timestamp);
      ++database_index_;
    }

    return state.to_result();
  }

  bool read_next_row(RowType& row) {
    PyObject* next = PyIter_Next(iterator_.ptr());
    if (next == nullptr) {
      if (PyErr_Occurred()) {
        throw nanobind::python_error();
      }
      return false;
    }

    nanobind::object row_object = nanobind::steal<nanobind::object>(next);
    row = nanobind::cast<RowType>(row_object);
    return true;
  }

  nanobind::object iterator_;
  nanobind::object database_owner_;
  const DatabaseType* database_ = nullptr;
  std::size_t database_index_ = 0;
  std::uint64_t interval_ns_ = 0;
  std::uint64_t offset_ns_ = 0;
  RowType pending_row_;
  bool has_pending_row_ = false;
};

using StockTradeAggregator = WindowAggregator<StockTradeAggregationTraits>;
using StockQuoteAggregator = WindowAggregator<StockQuoteAggregationTraits>;
using CurrencyQuoteAggregator = WindowAggregator<CurrencyQuoteAggregationTraits>;

template <typename RowType>
inline std::uint64_t participant_timestamp_at(const void* packed_data) {
  return RowType::participant_timestamp_at(packed_data);
}

template <typename RowType>
inline std::uint64_t sip_timestamp_at(const void* packed_data) {
  return RowType::sip_timestamp_at(packed_data);
}

struct StockTradeDatabaseTraits {
  using row_type = StockTrade;
  static constexpr std::string_view record_type = "stock_trade";

  static std::uint64_t search_timestamp_at(const void* packed_data) {
    return StockTrade::sip_timestamp_at(packed_data);
  }
};

struct StockQuoteDatabaseTraits {
  using row_type = StockQuote;
  static constexpr std::string_view record_type = "stock_quote";

  static std::uint64_t search_timestamp_at(const void* packed_data) {
    return StockQuote::sip_timestamp_at(packed_data);
  }
};

struct CurrencyQuoteDatabaseTraits {
  using row_type = CurrencyQuote;
  static constexpr std::string_view record_type = "currency_quote";

  static std::uint64_t search_timestamp_at(const void* packed_data) {
    return CurrencyQuote::participant_timestamp_at(packed_data);
  }
};

template <typename Traits>
class DatabaseRecordFile {
 public:
  using RowType = typename Traits::row_type;

  class Iterator {
   public:
    explicit Iterator(
        const DatabaseRecordFile& records,
        std::size_t start_index = 0,
        std::optional<std::uint64_t> stop_timestamp = std::nullopt)
        : records_(&records),
          index_(start_index),
          stop_timestamp_(stop_timestamp) {}

    Iterator& iter() { return *this; }

    RowType next() {
      if (index_ >= records_->size()) {
        throw nanobind::stop_iteration();
      }
      if (stop_timestamp_ && records_->timestamp_at(index_) > *stop_timestamp_) {
        throw nanobind::stop_iteration();
      }
      return records_->record_at(index_++);
    }

   private:
    const DatabaseRecordFile* records_;
    std::size_t index_ = 0;
    std::optional<std::uint64_t> stop_timestamp_;
  };

  DatabaseRecordFile(
      std::filesystem::path database_path,
      std::string date,
      std::string ticker)
      : database_path_(std::move(database_path)),
        date_(std::move(date)),
        ticker_(std::move(ticker)),
        file_path_(database_path_ / std::string(Traits::record_type) / date_ / ticker_),
        mapping_(file_path_) {
    if (mapping_.size() % RowType::packed_size != 0) {
      std::ostringstream message;
      message << "database file " << file_path_ << " has " << mapping_.size()
              << " bytes, which is not a multiple of fixed record size "
              << RowType::packed_size;
      throw std::invalid_argument(message.str());
    }
    size_ = mapping_.size() / RowType::packed_size;
  }

  const std::string& ticker() const { return ticker_; }
  const std::string& date() const { return date_; }
  const std::filesystem::path& database_path() const { return database_path_; }
  const std::filesystem::path& path() const { return file_path_; }
  std::size_t size() const { return size_; }

  const void* packed_data_at(std::size_t index) const {
    return mapping_.data_at(index * RowType::packed_size);
  }

  RowType get_item(std::int64_t index) const {
    return record_at(normalize_index(index));
  }

  Iterator iter() const {
    return Iterator(*this);
  }

  Iterator iterate_bounded(std::uint64_t start_timestamp) const {
    return Iterator(*this, galloping_lower_bound_timestamp(start_timestamp));
  }

  Iterator iterate_bounded(
      std::uint64_t start_timestamp,
      std::uint64_t stop_timestamp) const {
    return Iterator(
        *this,
        galloping_lower_bound_timestamp(start_timestamp),
        stop_timestamp);
  }

  std::int64_t index_before_timestamp(
      std::uint64_t timestamp,
      bool galloping = false) const {
    if (size_ == 0 || timestamp_at(0) > timestamp) {
      return -1;
    }

    if (galloping) {
      return galloping_index_before_timestamp(timestamp);
    }
    return binary_index_before_timestamp(0, size_, timestamp);
  }

  RowType find_before_participant_timestamp(
      std::uint64_t timestamp,
      std::uint64_t fuzz = 1'000'000'000ULL,
      bool galloping = false,
      bool on = true) const {
    return record_at(find_participant_timestamp_index(
        timestamp,
        fuzz,
        galloping,
        on,
        ParticipantSearchDirection::Before));
  }

  RowType find_after_participant_timestamp(
      std::uint64_t timestamp,
      std::uint64_t fuzz = 1'000'000'000ULL,
      bool galloping = false,
      bool on = true) const {
    return record_at(find_participant_timestamp_index(
        timestamp,
        fuzz,
        galloping,
        on,
        ParticipantSearchDirection::After));
  }

 protected:
  RowType record_at(std::size_t index) const {
    const auto view = std::string_view(
        mapping_.char_data_at(index * RowType::packed_size),
        RowType::packed_size);
    return RowType::from_packed(view, ticker_);
  }

  std::uint64_t timestamp_at(std::size_t index) const {
    return Traits::search_timestamp_at(
        mapping_.data_at(index * RowType::packed_size));
  }

 private:
  enum class ParticipantSearchDirection {
    Before,
    After,
  };

  std::size_t normalize_index(std::int64_t index) const {
    const auto signed_size = static_cast<std::int64_t>(size_);
    if (index < 0) {
      index += signed_size;
    }
    if (index < 0 || index >= signed_size) {
      throw std::out_of_range("database record index out of range");
    }
    return static_cast<std::size_t>(index);
  }

  std::int64_t binary_index_before_timestamp(
      std::size_t begin,
      std::size_t end,
      std::uint64_t timestamp) const {
    while (begin < end) {
      const std::size_t midpoint = begin + ((end - begin) / 2);
      if (timestamp_at(midpoint) <= timestamp) {
        begin = midpoint + 1;
      } else {
        end = midpoint;
      }
    }
    return static_cast<std::int64_t>(begin) - 1;
  }

  std::int64_t galloping_index_before_timestamp(std::uint64_t timestamp) const {
    std::size_t lower = 0;
    std::size_t upper = 1;

    while (upper < size_ && timestamp_at(upper) <= timestamp) {
      lower = upper;
      upper *= 2;
    }

    upper = std::min(upper + 1, size_);
    return binary_index_before_timestamp(lower, upper, timestamp);
  }

  std::size_t lower_bound_timestamp(
      std::size_t begin,
      std::size_t end,
      std::uint64_t timestamp) const {
    while (begin < end) {
      const std::size_t midpoint = begin + ((end - begin) / 2);
      if (timestamp_at(midpoint) < timestamp) {
        begin = midpoint + 1;
      } else {
        end = midpoint;
      }
    }
    return begin;
  }

  std::uint64_t participant_timestamp_at(std::size_t index) const {
    return RowType::participant_timestamp_at(
        mapping_.data_at(index * RowType::packed_size));
  }

  std::size_t participant_scan_lower_bound(
      std::uint64_t timestamp,
      std::uint64_t fuzz,
      bool galloping) const {
    const std::uint64_t lower_timestamp =
        (fuzz > timestamp) ? 0 : timestamp - fuzz;
    const auto prior_index = index_before_timestamp(timestamp, galloping);
    std::size_t lower = prior_index < 0 ? 0 : static_cast<std::size_t>(prior_index);

    while (lower > 0 && timestamp_at(lower - 1) >= lower_timestamp) {
      --lower;
    }
    while (lower < size_ && timestamp_at(lower) < lower_timestamp) {
      ++lower;
    }

    return lower;
  }

  std::size_t participant_scan_upper_bound(
      std::size_t lower,
      std::uint64_t timestamp,
      std::uint64_t fuzz) const {
    const std::uint64_t maximum_timestamp = std::numeric_limits<std::uint64_t>::max();
    const std::uint64_t upper_timestamp =
        (maximum_timestamp - timestamp < fuzz) ? maximum_timestamp : timestamp + fuzz;

    std::size_t upper = lower;
    while (upper < size_ && timestamp_at(upper) <= upper_timestamp) {
      ++upper;
    }

    return upper;
  }

  std::size_t find_participant_timestamp_index(
      std::uint64_t timestamp,
      std::uint64_t fuzz,
      bool galloping,
      bool on,
      ParticipantSearchDirection direction) const {
    if (size_ == 0) {
      throw std::out_of_range("cannot search an empty database record file");
    }

    const std::size_t lower =
        participant_scan_lower_bound(timestamp, fuzz, galloping);
    const std::size_t upper =
        participant_scan_upper_bound(lower, timestamp, fuzz);

    std::optional<std::size_t> best_index;
    std::uint64_t best_timestamp =
        direction == ParticipantSearchDirection::After
            ? std::numeric_limits<std::uint64_t>::max()
            : 0;

    for (std::size_t index = lower; index < upper; ++index) {
      const std::uint64_t candidate_timestamp = participant_timestamp_at(index);

      if (direction == ParticipantSearchDirection::After) {
        const bool matches =
            on ? candidate_timestamp >= timestamp : candidate_timestamp > timestamp;
        if (matches &&
            (!best_index || candidate_timestamp < best_timestamp)) {
          best_index = index;
          best_timestamp = candidate_timestamp;
        }
      } else {
        const bool matches =
            on ? candidate_timestamp <= timestamp : candidate_timestamp < timestamp;
        if (matches &&
            (!best_index || candidate_timestamp > best_timestamp)) {
          best_index = index;
          best_timestamp = candidate_timestamp;
        }
      }
    }

    if (!best_index) {
      throw std::out_of_range(
          "no record matched participant timestamp search bounds");
    }

    return *best_index;
  }

  std::size_t galloping_lower_bound_timestamp(std::uint64_t timestamp) const {
    if (size_ == 0) {
      return 0;
    }

    std::size_t lower = 0;
    std::size_t upper = 1;
    while (upper < size_ && timestamp_at(upper - 1) < timestamp) {
      lower = upper;
      upper = std::min(upper * 2, size_);
    }

    return lower_bound_timestamp(lower, upper, timestamp);
  }

  std::filesystem::path database_path_;
  std::string date_;
  std::string ticker_;
  std::filesystem::path file_path_;
  detail::MappedFile mapping_;
  std::size_t size_ = 0;
};

class StockMarketCalendarMixin {
 protected:
  static std::uint64_t market_timestamp_ns(
      const std::string& date,
      const char* field_name) {
    namespace nb = nanobind;
    nb::gil_scoped_acquire acquire;
    nb::module_ pmc = nb::module_::import_("pandas_market_calendars");
    nb::object calendar = pmc.attr("get_calendar")("NYSE");
    nb::object schedule = calendar.attr("schedule")(
        nb::arg("start_date") = date,
        nb::arg("end_date") = date);

    if (nb::cast<bool>(schedule.attr("empty"))) {
      std::ostringstream message;
      message << "no NYSE market session for date " << date;
      throw std::invalid_argument(message.str());
    }

    nb::object row = schedule.attr("iloc").attr("__getitem__")(0);
    nb::object timestamp = row.attr("__getitem__")(field_name);
    return nb::cast<std::uint64_t>(timestamp.attr("value"));
  }
};

class StockTradeDatabase
    : public DatabaseRecordFile<StockTradeDatabaseTraits>,
      public StockMarketCalendarMixin {
 public:
  using DatabaseRecordFile::DatabaseRecordFile;

  std::uint64_t market_open() const {
    return market_timestamp_ns(date(), "market_open");
  }

  std::uint64_t market_close() const {
    return market_timestamp_ns(date(), "market_close");
  }
};

class StockQuoteDatabase
    : public DatabaseRecordFile<StockQuoteDatabaseTraits>,
      public StockMarketCalendarMixin {
 public:
  using DatabaseRecordFile::DatabaseRecordFile;

  std::uint64_t market_open() const {
    return market_timestamp_ns(date(), "market_open");
  }

  std::uint64_t market_close() const {
    return market_timestamp_ns(date(), "market_close");
  }
};

class CurrencyQuoteDatabase
    : public DatabaseRecordFile<CurrencyQuoteDatabaseTraits> {
 public:
  using DatabaseRecordFile::DatabaseRecordFile;
};

struct NativeSpecialization {
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

template <typename Base, typename Specialization = NativeSpecialization>
class Implementation : public Base {
 public:
  using Base::Base;
  using specialization_type = Specialization;
  using GzipLineGenerator = std::generator<std::string>;
  using GzipLineIteratorType = decltype(std::declval<GzipLineGenerator&>().begin());
  using RawStockTrade = std::array<std::string, 13>;
  using RawStockQuote = std::array<std::string, 14>;
  using RawStockAggregate = std::array<std::string, 8>;
  using RawCurrencyQuote = std::array<std::string, 6>;
  using RawCurrencyAggregate = std::array<std::string, 8>;

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

        row = Implementation::parse_trade_row(line, bitset_cache_);
        return true;
      }

      return false;
    }

   private:
    detail::BufferedGzipLineReader reader_;
    bool is_first_line_ = true;
    detail::BitsetParseCache<96> bitset_cache_;
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

        row = Implementation::parse_quote_row(line, bitset_cache_);
        return true;
      }

      return false;
    }

   private:
    detail::BufferedGzipLineReader reader_;
    bool is_first_line_ = true;
    detail::BitsetParseCache<96> bitset_cache_;
  };

  class CurrencyQuoteStreamState {
   public:
    explicit CurrencyQuoteStreamState(const std::filesystem::path& path)
        : reader_(path) {}

    bool next_row(CurrencyQuote& row) {
      std::string_view line;

      while (reader_.template next_line<Specialization>(line)) {
        if (is_first_line_) {
          is_first_line_ = false;
          continue;
        }

        if (line.empty()) {
          continue;
        }

        row = Implementation::parse_currency_quote_row(line);
        return true;
      }

      return false;
    }

   private:
    detail::BufferedGzipLineReader reader_;
    bool is_first_line_ = true;
  };

  class StockAggregateStreamState {
   public:
    explicit StockAggregateStreamState(const std::filesystem::path& path)
        : reader_(path) {}

    bool next_row(StockAggregate& row) {
      std::string_view line;

      while (reader_.template next_line<Specialization>(line)) {
        if (is_first_line_) {
          is_first_line_ = false;
          continue;
        }

        if (line.empty()) {
          continue;
        }

        row = Implementation::parse_stock_aggregate_row(line);
        return true;
      }

      return false;
    }

   private:
    detail::BufferedGzipLineReader reader_;
    bool is_first_line_ = true;
  };

  class CurrencyAggregateStreamState {
   public:
    explicit CurrencyAggregateStreamState(const std::filesystem::path& path)
        : reader_(path) {}

    bool next_row(CurrencyAggregate& row) {
      std::string_view line;

      while (reader_.template next_line<Specialization>(line)) {
        if (is_first_line_) {
          is_first_line_ = false;
          continue;
        }

        if (line.empty()) {
          continue;
        }

        row = Implementation::parse_currency_aggregate_row(line);
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

  class RawCurrencyQuoteStreamState {
   public:
    explicit RawCurrencyQuoteStreamState(const std::filesystem::path& path)
        : reader_(path) {}

    RawCurrencyQuoteStreamState(const RawCurrencyQuoteStreamState&) = delete;
    RawCurrencyQuoteStreamState& operator=(const RawCurrencyQuoteStreamState&) = delete;

    RawCurrencyQuoteStreamState(RawCurrencyQuoteStreamState&& other) noexcept
        : reader_(std::move(other.reader_)),
          is_first_line_(other.is_first_line_),
          intern_cache_(std::move(other.intern_cache_)) {}

    RawCurrencyQuoteStreamState& operator=(RawCurrencyQuoteStreamState&& other) noexcept {
      if (this == &other) {
        return *this;
      }

      reader_ = std::move(other.reader_);
      is_first_line_ = other.is_first_line_;
      intern_cache_ = std::move(other.intern_cache_);
      return *this;
    }

    bool next_row(RawCurrencyQuote& row) {
      std::string_view line;

      while (reader_.template next_line<Specialization>(line)) {
        if (is_first_line_) {
          is_first_line_ = false;
          continue;
        }

        if (line.empty()) {
          continue;
        }

        row = Implementation::parse_raw_currency_quote_row(line);
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

        row = Implementation::parse_raw_currency_quote_tuple(line, intern_cache_);
        return true;
      }

      return false;
    }

   private:
    detail::BufferedGzipLineReader reader_;
    bool is_first_line_ = true;
    detail::RawBytesInternCache intern_cache_;
  };

  class RawStockAggregateStreamState {
   public:
    explicit RawStockAggregateStreamState(const std::filesystem::path& path)
        : reader_(path) {}

    RawStockAggregateStreamState(const RawStockAggregateStreamState&) = delete;
    RawStockAggregateStreamState& operator=(const RawStockAggregateStreamState&) = delete;

    RawStockAggregateStreamState(RawStockAggregateStreamState&& other) noexcept
        : reader_(std::move(other.reader_)),
          is_first_line_(other.is_first_line_),
          intern_cache_(std::move(other.intern_cache_)) {}

    RawStockAggregateStreamState& operator=(RawStockAggregateStreamState&& other) noexcept {
      if (this == &other) {
        return *this;
      }

      reader_ = std::move(other.reader_);
      is_first_line_ = other.is_first_line_;
      intern_cache_ = std::move(other.intern_cache_);
      return *this;
    }

    bool next_row(RawStockAggregate& row) {
      std::string_view line;

      while (reader_.template next_line<Specialization>(line)) {
        if (is_first_line_) {
          is_first_line_ = false;
          continue;
        }

        if (line.empty()) {
          continue;
        }

        row = Implementation::parse_raw_stock_aggregate_row(line);
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

        row = Implementation::parse_raw_stock_aggregate_tuple(line, intern_cache_);
        return true;
      }

      return false;
    }

   private:
    detail::BufferedGzipLineReader reader_;
    bool is_first_line_ = true;
    detail::RawBytesInternCache intern_cache_;
  };

  class RawCurrencyAggregateStreamState {
   public:
    explicit RawCurrencyAggregateStreamState(const std::filesystem::path& path)
        : reader_(path) {}

    RawCurrencyAggregateStreamState(const RawCurrencyAggregateStreamState&) = delete;
    RawCurrencyAggregateStreamState& operator=(const RawCurrencyAggregateStreamState&) = delete;

    RawCurrencyAggregateStreamState(RawCurrencyAggregateStreamState&& other) noexcept
        : reader_(std::move(other.reader_)),
          is_first_line_(other.is_first_line_),
          intern_cache_(std::move(other.intern_cache_)) {}

    RawCurrencyAggregateStreamState& operator=(RawCurrencyAggregateStreamState&& other) noexcept {
      if (this == &other) {
        return *this;
      }

      reader_ = std::move(other.reader_);
      is_first_line_ = other.is_first_line_;
      intern_cache_ = std::move(other.intern_cache_);
      return *this;
    }

    bool next_row(RawCurrencyAggregate& row) {
      std::string_view line;

      while (reader_.template next_line<Specialization>(line)) {
        if (is_first_line_) {
          is_first_line_ = false;
          continue;
        }

        if (line.empty()) {
          continue;
        }

        row = Implementation::parse_raw_currency_aggregate_row(line);
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

        row = Implementation::parse_raw_currency_aggregate_tuple(line, intern_cache_);
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
        detail::BitsetParseCache<96> bitset_cache;
        rows_ = collect_rows<StockTrade>(
            path,
            sort_by_participant_timestamp,
            sort_by_sip_timestamp,
            [&bitset_cache](std::string_view line) {
              return Implementation::parse_trade_row(line, bitset_cache);
            });
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
        detail::BitsetParseCache<96> bitset_cache;
        rows_ = collect_rows<StockQuote>(
            path,
            sort_by_participant_timestamp,
            sort_by_sip_timestamp,
            [&bitset_cache](std::string_view line) {
              return Implementation::parse_quote_row(line, bitset_cache);
            });
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

  class CurrencyQuoteRowsIterator {
   public:
    explicit CurrencyQuoteRowsIterator(
        const std::filesystem::path& path,
        bool sort_by_participant_timestamp = false,
        bool sort_by_sip_timestamp = false) {
      validate_currency_sort_flags(sort_by_participant_timestamp, sort_by_sip_timestamp);

      if (sort_by_participant_timestamp) {
        rows_ = collect_rows<CurrencyQuote>(
            path,
            sort_by_participant_timestamp,
            false,
            &Implementation::parse_currency_quote_row);
      } else {
        stream_state_.emplace(path);
      }
    }

    CurrencyQuoteRowsIterator& iter() { return *this; }

    CurrencyQuote next() {
      return Implementation::template next_parsed_row<CurrencyQuote, CurrencyQuoteStreamState>(
          stream_state_,
          rows_,
          row_index_);
    }

   private:
    std::optional<CurrencyQuoteStreamState> stream_state_;
    std::vector<CurrencyQuote> rows_;
    std::size_t row_index_ = 0;
  };

  class StockAggregateRowsIterator {
   public:
    explicit StockAggregateRowsIterator(
        const std::filesystem::path& path,
        bool sort_by_window_start = false) {
      if (sort_by_window_start) {
        rows_ = load_rows<StockAggregate>(path, &Implementation::parse_stock_aggregate_row);
        std::stable_sort(
            rows_.begin(),
            rows_.end(),
            [](const StockAggregate& lhs, const StockAggregate& rhs) {
              if (lhs.window_start != rhs.window_start) {
                return lhs.window_start < rhs.window_start;
              }
              return lhs.ticker < rhs.ticker;
            });
      } else {
        stream_state_.emplace(path);
      }
    }

    StockAggregateRowsIterator& iter() { return *this; }

    StockAggregate next() {
      return Implementation::template next_parsed_row<StockAggregate, StockAggregateStreamState>(
          stream_state_,
          rows_,
          row_index_);
    }

   private:
    std::optional<StockAggregateStreamState> stream_state_;
    std::vector<StockAggregate> rows_;
    std::size_t row_index_ = 0;
  };

  class CurrencyAggregateRowsIterator {
   public:
    explicit CurrencyAggregateRowsIterator(
        const std::filesystem::path& path,
        bool sort_by_window_start = false) {
      if (sort_by_window_start) {
        rows_ = load_rows<CurrencyAggregate>(path, &Implementation::parse_currency_aggregate_row);
        std::stable_sort(
            rows_.begin(),
            rows_.end(),
            [](const CurrencyAggregate& lhs, const CurrencyAggregate& rhs) {
              if (lhs.window_start != rhs.window_start) {
                return lhs.window_start < rhs.window_start;
              }
              return lhs.ticker < rhs.ticker;
            });
      } else {
        stream_state_.emplace(path);
      }
    }

    CurrencyAggregateRowsIterator& iter() { return *this; }

    CurrencyAggregate next() {
      return Implementation::template next_parsed_row<CurrencyAggregate, CurrencyAggregateStreamState>(
          stream_state_,
          rows_,
          row_index_);
    }

   private:
    std::optional<CurrencyAggregateStreamState> stream_state_;
    std::vector<CurrencyAggregate> rows_;
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

  class RawCurrencyQuoteRowsIterator {
   public:
    explicit RawCurrencyQuoteRowsIterator(
        const std::filesystem::path& path,
        bool sort_by_participant_timestamp = false,
        bool sort_by_sip_timestamp = false) {
      validate_currency_sort_flags(sort_by_participant_timestamp, sort_by_sip_timestamp);

      if (sort_by_participant_timestamp) {
        rows_ = collect_rows<RawCurrencyQuote>(
            path,
            sort_by_participant_timestamp,
            false,
            &Implementation::parse_raw_currency_quote_row);
      } else {
        stream_state_.emplace(path);
      }
    }

    RawCurrencyQuoteRowsIterator(const RawCurrencyQuoteRowsIterator&) = delete;
    RawCurrencyQuoteRowsIterator& operator=(const RawCurrencyQuoteRowsIterator&) = delete;

    RawCurrencyQuoteRowsIterator(RawCurrencyQuoteRowsIterator&& other) noexcept
        : stream_state_(std::move(other.stream_state_)),
          rows_(std::move(other.rows_)),
          row_index_(other.row_index_),
          intern_cache_(std::move(other.intern_cache_)) {}

    RawCurrencyQuoteRowsIterator& operator=(RawCurrencyQuoteRowsIterator&& other) noexcept {
      if (this == &other) {
        return *this;
      }

      stream_state_ = std::move(other.stream_state_);
      rows_ = std::move(other.rows_);
      row_index_ = other.row_index_;
      intern_cache_ = std::move(other.intern_cache_);
      return *this;
    }

    RawCurrencyQuoteRowsIterator& iter() { return *this; }

    nanobind::tuple next() {
      if (stream_state_) {
        nanobind::tuple row;
        if (stream_state_->next_tuple(row)) {
          return row;
        }

        stream_state_.reset();
        throw nanobind::stop_iteration();
      }

      return Implementation::raw_currency_quote_array_to_tuple(rows_next(), intern_cache_);
    }

   private:
    RawCurrencyQuote rows_next() {
      if (row_index_ >= rows_.size()) {
        throw nanobind::stop_iteration();
      }
      return std::move(rows_[row_index_++]);
    }

    std::optional<RawCurrencyQuoteStreamState> stream_state_;
    std::vector<RawCurrencyQuote> rows_;
    std::size_t row_index_ = 0;
    detail::RawBytesInternCache intern_cache_;
  };

  class RawStockAggregateRowsIterator {
   public:
    explicit RawStockAggregateRowsIterator(
        const std::filesystem::path& path,
        bool sort_by_window_start = false) {
      if (sort_by_window_start) {
        rows_ = load_rows<RawStockAggregate>(
            path,
            &Implementation::parse_raw_stock_aggregate_row);
        std::stable_sort(
            rows_.begin(),
            rows_.end(),
            [](const RawStockAggregate& lhs, const RawStockAggregate& rhs) {
              const auto lhs_window_start =
                  Specialization::template parse_integer<std::uint64_t>(lhs[6], "window_start");
              const auto rhs_window_start =
                  Specialization::template parse_integer<std::uint64_t>(rhs[6], "window_start");
              if (lhs_window_start != rhs_window_start) {
                return lhs_window_start < rhs_window_start;
              }
              return lhs[0] < rhs[0];
            });
      } else {
        stream_state_.emplace(path);
      }
    }

    RawStockAggregateRowsIterator(const RawStockAggregateRowsIterator&) = delete;
    RawStockAggregateRowsIterator& operator=(const RawStockAggregateRowsIterator&) = delete;

    RawStockAggregateRowsIterator(RawStockAggregateRowsIterator&& other) noexcept
        : stream_state_(std::move(other.stream_state_)),
          rows_(std::move(other.rows_)),
          row_index_(other.row_index_),
          intern_cache_(std::move(other.intern_cache_)) {}

    RawStockAggregateRowsIterator& operator=(RawStockAggregateRowsIterator&& other) noexcept {
      if (this == &other) {
        return *this;
      }

      stream_state_ = std::move(other.stream_state_);
      rows_ = std::move(other.rows_);
      row_index_ = other.row_index_;
      intern_cache_ = std::move(other.intern_cache_);
      return *this;
    }

    RawStockAggregateRowsIterator& iter() { return *this; }

    nanobind::tuple next() {
      if (stream_state_) {
        nanobind::tuple row;
        if (stream_state_->next_tuple(row)) {
          return row;
        }

        stream_state_.reset();
        throw nanobind::stop_iteration();
      }

      return Implementation::raw_stock_aggregate_array_to_tuple(rows_next(), intern_cache_);
    }

   private:
    RawStockAggregate rows_next() {
      if (row_index_ >= rows_.size()) {
        throw nanobind::stop_iteration();
      }
      return std::move(rows_[row_index_++]);
    }

    std::optional<RawStockAggregateStreamState> stream_state_;
    std::vector<RawStockAggregate> rows_;
    std::size_t row_index_ = 0;
    detail::RawBytesInternCache intern_cache_;
  };

  class RawCurrencyAggregateRowsIterator {
   public:
    explicit RawCurrencyAggregateRowsIterator(
        const std::filesystem::path& path,
        bool sort_by_window_start = false) {
      if (sort_by_window_start) {
        rows_ = load_rows<RawCurrencyAggregate>(
            path,
            &Implementation::parse_raw_currency_aggregate_row);
        std::stable_sort(
            rows_.begin(),
            rows_.end(),
            [](const RawCurrencyAggregate& lhs, const RawCurrencyAggregate& rhs) {
              const auto lhs_window_start =
                  Specialization::template parse_integer<std::uint64_t>(lhs[6], "window_start");
              const auto rhs_window_start =
                  Specialization::template parse_integer<std::uint64_t>(rhs[6], "window_start");
              if (lhs_window_start != rhs_window_start) {
                return lhs_window_start < rhs_window_start;
              }
              return lhs[0] < rhs[0];
            });
      } else {
        stream_state_.emplace(path);
      }
    }

    RawCurrencyAggregateRowsIterator(const RawCurrencyAggregateRowsIterator&) = delete;
    RawCurrencyAggregateRowsIterator& operator=(const RawCurrencyAggregateRowsIterator&) = delete;

    RawCurrencyAggregateRowsIterator(RawCurrencyAggregateRowsIterator&& other) noexcept
        : stream_state_(std::move(other.stream_state_)),
          rows_(std::move(other.rows_)),
          row_index_(other.row_index_),
          intern_cache_(std::move(other.intern_cache_)) {}

    RawCurrencyAggregateRowsIterator& operator=(RawCurrencyAggregateRowsIterator&& other) noexcept {
      if (this == &other) {
        return *this;
      }

      stream_state_ = std::move(other.stream_state_);
      rows_ = std::move(other.rows_);
      row_index_ = other.row_index_;
      intern_cache_ = std::move(other.intern_cache_);
      return *this;
    }

    RawCurrencyAggregateRowsIterator& iter() { return *this; }

    nanobind::tuple next() {
      if (stream_state_) {
        nanobind::tuple row;
        if (stream_state_->next_tuple(row)) {
          return row;
        }

        stream_state_.reset();
        throw nanobind::stop_iteration();
      }

      return Implementation::raw_currency_aggregate_array_to_tuple(rows_next(), intern_cache_);
    }

   private:
    RawCurrencyAggregate rows_next() {
      if (row_index_ >= rows_.size()) {
        throw nanobind::stop_iteration();
      }
      return std::move(rows_[row_index_++]);
    }

    std::optional<RawCurrencyAggregateStreamState> stream_state_;
    std::vector<RawCurrencyAggregate> rows_;
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
    detail::BitsetParseCache<96> bitset_cache;
    return collect_rows<StockTrade>(
        path,
        sort_by_participant_timestamp,
        sort_by_sip_timestamp,
        [&bitset_cache](std::string_view line) {
          return Implementation::parse_trade_row(line, bitset_cache);
        });
  }

  std::vector<StockQuote> parse_quote_rows(
      const std::filesystem::path& path,
      bool sort_by_participant_timestamp = false,
      bool sort_by_sip_timestamp = false) const {
    detail::BitsetParseCache<96> bitset_cache;
    return collect_rows<StockQuote>(
        path,
        sort_by_participant_timestamp,
        sort_by_sip_timestamp,
        [&bitset_cache](std::string_view line) {
          return Implementation::parse_quote_row(line, bitset_cache);
        });
  }

  std::vector<CurrencyQuote> parse_currency_quote_rows(
      const std::filesystem::path& path,
      bool sort_by_participant_timestamp = false,
      bool sort_by_sip_timestamp = false) const {
    validate_currency_sort_flags(sort_by_participant_timestamp, sort_by_sip_timestamp);
    return collect_rows<CurrencyQuote>(
        path,
        sort_by_participant_timestamp,
        false,
        &Implementation::parse_currency_quote_row);
  }

  std::vector<StockAggregate> parse_stock_aggregate_rows(
      const std::filesystem::path& path,
      bool sort_by_window_start = false) const {
    auto rows = load_rows<StockAggregate>(path, &Implementation::parse_stock_aggregate_row);
    if (sort_by_window_start) {
      std::stable_sort(rows.begin(), rows.end(), [](const StockAggregate& lhs, const StockAggregate& rhs) {
        if (lhs.window_start != rhs.window_start) {
          return lhs.window_start < rhs.window_start;
        }
        return lhs.ticker < rhs.ticker;
      });
    }
    return rows;
  }

  std::vector<CurrencyAggregate> parse_currency_aggregate_rows(
      const std::filesystem::path& path,
      bool sort_by_window_start = false) const {
    auto rows = load_rows<CurrencyAggregate>(path, &Implementation::parse_currency_aggregate_row);
    if (sort_by_window_start) {
      std::stable_sort(rows.begin(), rows.end(), [](const CurrencyAggregate& lhs, const CurrencyAggregate& rhs) {
        if (lhs.window_start != rhs.window_start) {
          return lhs.window_start < rhs.window_start;
        }
        return lhs.ticker < rhs.ticker;
      });
    }
    return rows;
  }

  static std::uint64_t build_database_file(
      const std::filesystem::path& input_path,
      const std::filesystem::path& database_path,
      std::string_view record_type) {
    if (record_type == "stock_trade") {
      detail::BitsetParseCache<96> bitset_cache;
      return build_parsed_database<StockTrade>(
          input_path,
          database_path,
          record_type,
          [&bitset_cache](std::string_view line) {
            return Implementation::parse_trade_row(line, bitset_cache);
          },
          [](const StockTrade& row) { return row.sip_timestamp; });
    }

    if (record_type == "stock_quote") {
      detail::BitsetParseCache<96> bitset_cache;
      return build_parsed_database<StockQuote>(
          input_path,
          database_path,
          record_type,
          [&bitset_cache](std::string_view line) {
            return Implementation::parse_quote_row(line, bitset_cache);
          },
          [](const StockQuote& row) { return row.sip_timestamp; });
    }

    if (record_type == "currency_quote") {
      return build_parsed_database<CurrencyQuote>(
          input_path,
          database_path,
          record_type,
          [](std::string_view line) {
            return Implementation::parse_currency_quote_row(line);
          },
          [](const CurrencyQuote& row) { return row.participant_timestamp; });
    }

    std::ostringstream message;
    message << "unsupported database record type: " << record_type;
    throw std::invalid_argument(message.str());
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
  template <typename RowType, typename ParseRowFn, typename DateTimestampFn>
  static std::uint64_t build_parsed_database(
      const std::filesystem::path& input_path,
      const std::filesystem::path& database_path,
      std::string_view record_type,
      ParseRowFn parse_row,
      DateTimestampFn date_timestamp) {
    std::filesystem::create_directories(database_path);

    detail::BufferedGzipLineReader reader(input_path);
    detail::BinaryRecordWriter writer;
    std::string_view line;
    std::filesystem::path output_root;
    std::string current_ticker;
    bool is_first_line = true;
    bool has_output_root = false;
    bool has_current_output = false;
    std::uint64_t rows_written = 0;

    while (reader.template next_line<Specialization>(line)) {
      if (is_first_line) {
        is_first_line = false;
        continue;
      }

      if (line.empty()) {
        continue;
      }

      const RowType row = parse_row(line);
      if (!has_output_root) {
        output_root = database_path / std::string(record_type) /
            detail::utc_date_directory_name(date_timestamp(row));
        std::filesystem::create_directories(output_root);
        has_output_root = true;
      }

      if (!has_current_output || row.ticker != current_ticker) {
        current_ticker = row.ticker;
        writer.open(output_root / current_ticker);
        has_current_output = true;
      }

      writer.write(row.pack());
      ++rows_written;
    }

    writer.close();
    return rows_written;
  }

  static StockTrade parse_trade_row(
      std::string_view line,
      detail::BitsetParseCache<96>& bitset_cache) {
    detail::CsvLineCursor cursor(line);
    std::string scratch;
    StockTrade result;

    result.ticker.assign(cursor.template next_field<Specialization, true>(scratch));
    result.conditions = bitset_cache.get_or_parse(
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

  static StockQuote parse_quote_row(
      std::string_view line,
      detail::BitsetParseCache<96>& bitset_cache) {
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
    result.conditions = bitset_cache.get_or_parse(
        cursor.template next_field<Specialization, true>(scratch),
        "conditions");
    result.indicators = bitset_cache.get_or_parse(
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

  static CurrencyQuote parse_currency_quote_row(std::string_view line) {
    detail::CsvLineCursor cursor(line);
    std::string scratch;
    CurrencyQuote result;

    result.ticker.assign(cursor.template next_field<Specialization, true>(scratch));
    result.ask_exchange =
        Specialization::template parse_integer<std::uint8_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "ask_exchange");
    result.ask_price = Specialization::parse_double(
        cursor.template next_field<Specialization, true>(scratch),
        "ask_price");
    result.bid_exchange =
        Specialization::template parse_integer<std::uint8_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "bid_exchange");
    result.bid_price = Specialization::parse_double(
        cursor.template next_field<Specialization, true>(scratch),
        "bid_price");
    result.participant_timestamp =
        Specialization::template parse_integer<std::uint64_t>(
            cursor.template next_field<Specialization, false>(scratch),
            "participant_timestamp");

    cursor.finish();
    return result;
  }

  static StockAggregate parse_stock_aggregate_row(std::string_view line) {
    detail::CsvLineCursor cursor(line);
    std::string scratch;
    StockAggregate result;

    result.ticker.assign(cursor.template next_field<Specialization, true>(scratch));
    result.volume =
        Specialization::template parse_integer<std::uint64_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "volume");
    result.open = Specialization::parse_double(
        cursor.template next_field<Specialization, true>(scratch),
        "open");
    result.close = Specialization::parse_double(
        cursor.template next_field<Specialization, true>(scratch),
        "close");
    result.high = Specialization::parse_double(
        cursor.template next_field<Specialization, true>(scratch),
        "high");
    result.low = Specialization::parse_double(
        cursor.template next_field<Specialization, true>(scratch),
        "low");
    result.window_start =
        Specialization::template parse_integer<std::uint64_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "window_start");
    result.transactions =
        Specialization::template parse_integer<std::uint64_t>(
            cursor.template next_field<Specialization, false>(scratch),
            "transactions");

    cursor.finish();
    return result;
  }

  static CurrencyAggregate parse_currency_aggregate_row(std::string_view line) {
    detail::CsvLineCursor cursor(line);
    std::string scratch;
    CurrencyAggregate result;

    result.ticker.assign(cursor.template next_field<Specialization, true>(scratch));
    result.volume =
        Specialization::template parse_integer<std::uint64_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "volume");
    result.open = Specialization::parse_double(
        cursor.template next_field<Specialization, true>(scratch),
        "open");
    result.close = Specialization::parse_double(
        cursor.template next_field<Specialization, true>(scratch),
        "close");
    result.high = Specialization::parse_double(
        cursor.template next_field<Specialization, true>(scratch),
        "high");
    result.low = Specialization::parse_double(
        cursor.template next_field<Specialization, true>(scratch),
        "low");
    result.window_start =
        Specialization::template parse_integer<std::uint64_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "window_start");
    result.transactions =
        Specialization::template parse_integer<std::uint64_t>(
            cursor.template next_field<Specialization, false>(scratch),
            "transactions");

    cursor.finish();
    return result;
  }

  static RawCurrencyQuote parse_raw_currency_quote_row(std::string_view line) {
    return parse_raw_row<6>(line);
  }

  static RawStockAggregate parse_raw_stock_aggregate_row(std::string_view line) {
    return parse_raw_row<8>(line);
  }

  static nanobind::tuple parse_raw_currency_quote_tuple(
      std::string_view line,
      detail::RawBytesInternCache& intern_cache) {
    std::size_t cursor = 0;
    nanobind::tuple result = make_raw_tuple<6>();

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
    set_raw_small_uint_field(
        result,
        3,
        next_raw_unquoted_field<true>(line, cursor),
        intern_cache);
    set_raw_bytes_field(result, 4, next_raw_unquoted_field<true>(line, cursor));
    set_raw_bytes_field(result, 5, next_raw_unquoted_field<false>(line, cursor));

    finish_raw_row(line, cursor);
    return result;
  }

  static RawCurrencyAggregate parse_raw_currency_aggregate_row(std::string_view line) {
    return parse_raw_row<8>(line);
  }

  static nanobind::tuple parse_raw_currency_aggregate_tuple(
      std::string_view line,
      detail::RawBytesInternCache& intern_cache) {
    std::size_t cursor = 0;
    nanobind::tuple result = make_raw_tuple<8>();

    set_raw_ticker_field(
        result,
        next_raw_unquoted_field<true>(line, cursor),
        intern_cache);
    set_raw_bytes_field(result, 1, next_raw_unquoted_field<true>(line, cursor));
    set_raw_bytes_field(result, 2, next_raw_unquoted_field<true>(line, cursor));
    set_raw_bytes_field(result, 3, next_raw_unquoted_field<true>(line, cursor));
    set_raw_bytes_field(result, 4, next_raw_unquoted_field<true>(line, cursor));
    set_raw_bytes_field(result, 5, next_raw_unquoted_field<true>(line, cursor));
    set_raw_bytes_field(result, 6, next_raw_unquoted_field<true>(line, cursor));
    set_raw_bytes_field(result, 7, next_raw_unquoted_field<false>(line, cursor));

    finish_raw_row(line, cursor);
    return result;
  }

  static nanobind::tuple parse_raw_stock_aggregate_tuple(
      std::string_view line,
      detail::RawBytesInternCache& intern_cache) {
    std::size_t cursor = 0;
    nanobind::tuple result = make_raw_tuple<8>();

    set_raw_ticker_field(
        result,
        next_raw_unquoted_field<true>(line, cursor),
        intern_cache);
    set_raw_bytes_field(result, 1, next_raw_unquoted_field<true>(line, cursor));
    set_raw_bytes_field(result, 2, next_raw_unquoted_field<true>(line, cursor));
    set_raw_bytes_field(result, 3, next_raw_unquoted_field<true>(line, cursor));
    set_raw_bytes_field(result, 4, next_raw_unquoted_field<true>(line, cursor));
    set_raw_bytes_field(result, 5, next_raw_unquoted_field<true>(line, cursor));
    set_raw_bytes_field(result, 6, next_raw_unquoted_field<true>(line, cursor));
    set_raw_bytes_field(result, 7, next_raw_unquoted_field<false>(line, cursor));

    finish_raw_row(line, cursor);
    return result;
  }

  static nanobind::tuple raw_currency_aggregate_array_to_tuple(
      const RawCurrencyAggregate& fields,
      detail::RawBytesInternCache& intern_cache) {
    nanobind::tuple result = make_raw_tuple<8>();
    set_raw_ticker_field(result, fields[0], intern_cache);
    set_raw_bytes_field(result, 1, fields[1]);
    set_raw_bytes_field(result, 2, fields[2]);
    set_raw_bytes_field(result, 3, fields[3]);
    set_raw_bytes_field(result, 4, fields[4]);
    set_raw_bytes_field(result, 5, fields[5]);
    set_raw_bytes_field(result, 6, fields[6]);
    set_raw_bytes_field(result, 7, fields[7]);
    return result;
  }

  static nanobind::tuple raw_stock_aggregate_array_to_tuple(
      const RawStockAggregate& fields,
      detail::RawBytesInternCache& intern_cache) {
    nanobind::tuple result = make_raw_tuple<8>();
    set_raw_ticker_field(result, fields[0], intern_cache);
    set_raw_bytes_field(result, 1, fields[1]);
    set_raw_bytes_field(result, 2, fields[2]);
    set_raw_bytes_field(result, 3, fields[3]);
    set_raw_bytes_field(result, 4, fields[4]);
    set_raw_bytes_field(result, 5, fields[5]);
    set_raw_bytes_field(result, 6, fields[6]);
    set_raw_bytes_field(result, 7, fields[7]);
    return result;
  }

  static nanobind::tuple raw_currency_quote_array_to_tuple(
      const RawCurrencyQuote& fields,
      detail::RawBytesInternCache& intern_cache) {
    nanobind::tuple result = make_raw_tuple<6>();
    set_raw_ticker_field(result, fields[0], intern_cache);
    set_raw_small_uint_field(result, 1, fields[1], intern_cache);
    set_raw_bytes_field(result, 2, fields[2]);
    set_raw_small_uint_field(result, 3, fields[3], intern_cache);
    set_raw_bytes_field(result, 4, fields[4]);
    set_raw_bytes_field(result, 5, fields[5]);
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

  static void validate_currency_sort_flags(
      bool sort_by_participant_timestamp,
      bool sort_by_sip_timestamp) {
    if (sort_by_sip_timestamp) {
      throw std::invalid_argument(
          "currency quotes do not support sort_by_sip_timestamp");
    }
    static_cast<void>(sort_by_participant_timestamp);
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

  static std::uint64_t participant_timestamp_value(const RawCurrencyQuote& row) {
    return Specialization::template parse_integer<std::uint64_t>(
        row[5],
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

  static std::uint64_t sip_timestamp_value(const RawCurrencyQuote& row) {
    return participant_timestamp_value(row);
  }

  static std::uint64_t sip_timestamp_value(const CurrencyQuote& row) {
    return row.participant_timestamp;
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

  static const std::string& ticker_value(const RawCurrencyQuote& row) {
    return row[0];
  }

  template <typename RowType>
  static const std::string& ticker_value(const RowType& row) {
    return row.ticker;
  }

  template <typename RowType, typename ParseRowFn>
  static std::vector<RowType> collect_rows(
      const std::filesystem::path& path,
      bool sort_by_participant_timestamp,
      bool sort_by_sip_timestamp,
      ParseRowFn parse_row) {
    validate_sort_flags(sort_by_participant_timestamp, sort_by_sip_timestamp);

    if (sort_by_participant_timestamp) {
      auto rows = load_rows<RowType>(path, parse_row);
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

    auto rows = load_rows<RowType>(path, parse_row);
    if (sort_by_sip_timestamp) {
      std::sort(rows.begin(), rows.end(), [](const RowType& lhs, const RowType& rhs) {
        const auto lhs_sip_timestamp = Implementation::sip_timestamp_value(lhs);
        const auto rhs_sip_timestamp = Implementation::sip_timestamp_value(rhs);
        if (lhs_sip_timestamp != rhs_sip_timestamp) {
          return lhs_sip_timestamp < rhs_sip_timestamp;
        }
        if (Implementation::ticker_value(lhs) != Implementation::ticker_value(rhs)) {
          return Implementation::ticker_value(lhs) < Implementation::ticker_value(rhs);
        }
        return Implementation::participant_timestamp_value(lhs) <
            Implementation::participant_timestamp_value(rhs);
      });
    }
    return rows;
  }

  template <typename RowType, typename ParseRowFn>
  static std::vector<RowType> load_rows(
      const std::filesystem::path& path,
      ParseRowFn parse_row) {
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

};

}  // namespace massive_speedup::native
