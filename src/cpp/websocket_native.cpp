#include "websocket_native.hpp"

#include "massive_speedup/parsers.hpp"

#include <Python.h>

#include <algorithm>
#include <bitset>
#include <charconv>
#include <chrono>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#if MASSIVE_SPEEDUP_HAS_SIMDJSON
  #include <simdjson.h>
#endif

namespace massive_speedup::native {

namespace websocket_detail {

#if MASSIVE_SPEEDUP_HAS_SIMDJSON

[[noreturn]] void throw_json_error(
    std::string_view context,
    simdjson::error_code error) {
  std::ostringstream message;
  message << context << ": " << simdjson::error_message(error);
  throw std::invalid_argument(message.str());
}

void require_success(
    simdjson::error_code error,
    std::string_view context) {
  if (error) {
    throw_json_error(context, error);
  }
}

#endif

class MessageState {
 public:
#if MASSIVE_SPEEDUP_HAS_SIMDJSON
  MessageState(std::string_view payload, WebSocketAsset asset)
      : payload_(payload), asset_(asset) {}

  const simdjson::padded_string& payload() const noexcept { return payload_; }

  simdjson::padded_string_view view(
      std::size_t offset,
      std::size_t length) const {
    if (offset > payload_.size() || length > payload_.size() - offset) {
      throw std::out_of_range("websocket event is outside its message buffer");
    }
    return simdjson::padded_string_view(
        payload_.data() + offset,
        length,
        payload_.size() + simdjson::SIMDJSON_PADDING - offset);
  }
#else
  MessageState(std::string_view payload, WebSocketAsset asset)
      : payload_(payload), asset_(asset) {}
#endif

  std::string_view bytes() const noexcept {
#if MASSIVE_SPEEDUP_HAS_SIMDJSON
    return {payload_.data(), payload_.size()};
#else
    return payload_;
#endif
  }

  WebSocketAsset asset() const noexcept { return asset_; }

 private:
#if MASSIVE_SPEEDUP_HAS_SIMDJSON
  simdjson::padded_string payload_;
#else
  std::string payload_;
#endif
  WebSocketAsset asset_;
};

class EventState {
 public:
  EventState(
      std::shared_ptr<MessageState> message,
      std::size_t offset,
      std::size_t length,
      std::string event_type)
      : message_(std::move(message)),
        offset_(offset),
        length_(length),
        event_type_(std::move(event_type)) {}

  ~EventState() = default;

  const std::string& event_type() const noexcept { return event_type_; }

  std::string_view raw_json_view() const {
    const std::string_view message = message_->bytes();
    return message.substr(offset_, length_);
  }

  std::string_view message_view() const noexcept { return message_->bytes(); }

  nb::object required_field(std::string_view key) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto value = field_locked(key);
    if (!value.has_value()) {
      PyErr_SetObject(
          PyExc_KeyError,
          nb::str(key.data(), key.size()).ptr());
      throw nb::python_error();
    }
    return *value;
  }

  nb::object optional_field(
      std::string_view key,
      nb::handle default_value) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto value = field_locked(key);
    if (value.has_value()) {
      return *value;
    }
    return nb::borrow<nb::object>(default_value);
  }

  bool contains(std::string_view key) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return field_locked(key).has_value();
  }

  bool is_cached(std::string_view key) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return fields_.find(std::string(key)) != fields_.end();
  }

  bool is_property_cached(std::string_view key) const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return properties_.find(std::string(key)) != properties_.end();
  }

  nb::tuple cached_fields() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return string_tuple(access_order_);
  }

  nb::tuple cached_properties() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return string_tuple(property_access_order_);
  }

  nb::object cached_property(
      std::string_view key,
      const std::function<nb::object()>& factory) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    const std::string owned_key(key);
    const auto cached = properties_.find(owned_key);
    if (cached != properties_.end()) {
      return cached->second;
    }
    nb::object value = factory();
    properties_.emplace(owned_key, value);
    property_access_order_.push_back(owned_key);
    return value;
  }

 private:
  static nb::tuple string_tuple(const std::vector<std::string>& strings) {
    nb::tuple result = nb::steal<nb::tuple>(
        PyTuple_New(static_cast<Py_ssize_t>(strings.size())));
    if (!result.is_valid()) {
      throw nb::python_error();
    }
    for (std::size_t index = 0; index < strings.size(); ++index) {
      const std::string& key = strings[index];
      PyTuple_SET_ITEM(
          result.ptr(),
          static_cast<Py_ssize_t>(index),
          nb::str(key.data(), key.size()).release().ptr());
    }
    return result;
  }
  std::optional<nb::object> field_locked(std::string_view key) {
    const std::string owned_key(key);
    const auto cached = fields_.find(owned_key);
    if (cached != fields_.end()) {
      if (missing_fields_.contains(owned_key)) {
        return std::nullopt;
      }
      return cached->second;
    }

    if (key == "ev") {
      nb::object value = nb::str(event_type_.data(), event_type_.size());
      fields_.emplace(owned_key, value);
      access_order_.push_back(owned_key);
      return value;
    }

#if MASSIVE_SPEEDUP_HAS_SIMDJSON
    ensure_document_locked();
    simdjson::ondemand::value value;
    const simdjson::error_code error =
        document_->find_field_unordered(key).get(value);
    if (error == simdjson::NO_SUCH_FIELD) {
      fields_.emplace(owned_key, nb::none());
      missing_fields_.insert(owned_key);
      access_order_.push_back(owned_key);
      return std::nullopt;
    }
    require_success(error, "could not read websocket field");

    nb::object python_value = value_to_python(value);
    fields_.emplace(owned_key, python_value);
    access_order_.push_back(owned_key);
    return python_value;
#else
    throw std::runtime_error(
        "websocket parsing requires a build with simdjson support");
#endif
  }

#if MASSIVE_SPEEDUP_HAS_SIMDJSON
  void ensure_document_locked() {
    if (document_.has_value()) {
      return;
    }
    parser_ = std::make_unique<simdjson::ondemand::parser>();
    simdjson::ondemand::document document;
    require_success(
        parser_->iterate(message_->view(offset_, length_)).get(document),
        "could not parse websocket event");
    document_.emplace(std::move(document));
  }

  static nb::object value_to_python(simdjson::ondemand::value& value) {
    simdjson::ondemand::json_type type;
    require_success(value.type().get(type), "could not inspect websocket field");

    switch (type) {
      case simdjson::ondemand::json_type::string: {
        std::string_view string_value;
        require_success(
            value.get_string().get(string_value),
            "could not read websocket string field");
        return nb::str(string_value.data(), string_value.size());
      }
      case simdjson::ondemand::json_type::number: {
        simdjson::ondemand::number_type number_type;
        require_success(
            value.get_number_type().get(number_type),
            "could not inspect websocket number field");
        if (number_type == simdjson::ondemand::number_type::big_integer) {
          const std::string_view token = value.raw_json_token();
          std::string owned(token);
          char* end = nullptr;
          PyObject* integer = PyLong_FromString(owned.data(), &end, 10);
          if (integer == nullptr) {
            throw nb::python_error();
          }
          return nb::steal<nb::object>(integer);
        }

        simdjson::ondemand::number number;
        require_success(
            value.get_number().get(number),
            "could not read websocket number field");
        switch (number_type) {
          case simdjson::ondemand::number_type::signed_integer:
            return nb::int_(number.get_int64());
          case simdjson::ondemand::number_type::unsigned_integer:
            return nb::int_(number.get_uint64());
          case simdjson::ondemand::number_type::floating_point_number:
            return nb::float_(number.get_double());
          case simdjson::ondemand::number_type::big_integer:
            break;
        }
        throw std::logic_error("unhandled websocket number type");
      }
      case simdjson::ondemand::json_type::boolean: {
        bool bool_value = false;
        require_success(
            value.get_bool().get(bool_value),
            "could not read websocket boolean field");
        return nb::bool_(bool_value);
      }
      case simdjson::ondemand::json_type::null: {
        bool is_null = false;
        require_success(
            value.is_null().get(is_null),
            "could not read websocket null field");
        if (!is_null) {
          throw std::invalid_argument("websocket field changed type while parsing");
        }
        return nb::none();
      }
      case simdjson::ondemand::json_type::array: {
        simdjson::ondemand::array array;
        require_success(
            value.get_array().get(array),
            "could not read websocket array field");
        nb::list result;
        for (auto item_result : array) {
          simdjson::ondemand::value item;
          require_success(
              item_result.get(item),
              "could not read websocket array item");
          result.append(value_to_python(item));
        }
        return result;
      }
      case simdjson::ondemand::json_type::object: {
        simdjson::ondemand::object object;
        require_success(
            value.get_object().get(object),
            "could not read websocket object field");
        nb::dict result;
        for (auto field_result : object) {
          simdjson::ondemand::field field;
          require_success(
              std::move(field_result).get(field),
              "could not read websocket object member");
          std::string_view key;
          require_success(
              field.unescaped_key().get(key),
              "could not read websocket object key");
          result[nb::str(key.data(), key.size())] = value_to_python(field.value());
        }
        return result;
      }
    }
    throw std::logic_error("unhandled websocket JSON type");
  }
#endif

  std::shared_ptr<MessageState> message_;
#if MASSIVE_SPEEDUP_HAS_SIMDJSON
  std::unique_ptr<simdjson::ondemand::parser> parser_;
  std::optional<simdjson::ondemand::document> document_;
#endif
  std::size_t offset_ = 0;
  std::size_t length_ = 0;
  std::string event_type_;
  mutable std::recursive_mutex mutex_;
  std::unordered_map<std::string, nb::object> fields_;
  std::unordered_set<std::string> missing_fields_;
  std::vector<std::string> access_order_;
  std::unordered_map<std::string, nb::object> properties_;
  std::vector<std::string> property_access_order_;
};

#if MASSIVE_SPEEDUP_HAS_SIMDJSON

struct EventSlice {
  std::size_t offset = 0;
  std::size_t length = 0;
};

EventSlice make_slice(
    const MessageState& message,
    std::string_view raw_json) {
  const char* begin = message.payload().data();
  const char* end = begin + message.payload().size();
  if (raw_json.data() < begin || raw_json.data() > end ||
      raw_json.size() > static_cast<std::size_t>(end - raw_json.data())) {
    throw std::logic_error("simdjson returned an event outside its source message");
  }
  return {
      static_cast<std::size_t>(raw_json.data() - begin),
      raw_json.size()};
}

std::vector<EventSlice> find_event_slices(const MessageState& message) {
  simdjson::ondemand::parser parser;
  simdjson::ondemand::document document;
  require_success(
      parser.iterate(message.view(0, message.payload().size())).get(document),
      "could not parse websocket message");

  simdjson::ondemand::json_type root_type;
  require_success(
      document.type().get(root_type),
      "could not inspect websocket message");

  std::vector<EventSlice> slices;
  if (root_type == simdjson::ondemand::json_type::object) {
    simdjson::ondemand::object object;
    require_success(
        document.get_object().get(object),
        "websocket message root must be an object or array of objects");
    std::string_view raw_json;
    require_success(
        object.raw_json().get(raw_json),
        "could not read websocket event");
    slices.push_back(make_slice(message, raw_json));
    return slices;
  }

  if (root_type != simdjson::ondemand::json_type::array) {
    throw std::invalid_argument(
        "websocket message root must be an object or array of objects");
  }

  simdjson::ondemand::array array;
  require_success(
      document.get_array().get(array),
      "could not read websocket message array");
  for (auto value_result : array) {
    simdjson::ondemand::value value;
    require_success(
        value_result.get(value),
        "could not read websocket event");
    simdjson::ondemand::json_type value_type;
    require_success(
        value.type().get(value_type),
        "could not inspect websocket event");
    if (value_type != simdjson::ondemand::json_type::object) {
      throw std::invalid_argument(
          "every websocket message item must be a JSON object");
    }
    simdjson::ondemand::object object;
    require_success(
        value.get_object().get(object),
        "could not read websocket event object");
    std::string_view raw_json;
    require_success(
        object.raw_json().get(raw_json),
        "could not read websocket event object");
    slices.push_back(make_slice(message, raw_json));
  }
  return slices;
}

std::string classify_event(
    simdjson::ondemand::parser& parser,
    const MessageState& message,
    const EventSlice& slice) {
  simdjson::ondemand::document document;
  require_success(
      parser.iterate(message.view(slice.offset, slice.length)).get(document),
      "could not parse websocket event");

  simdjson::ondemand::value event_value;
  const simdjson::error_code event_error =
      document.find_field_unordered("ev").get(event_value);
  if (event_error == simdjson::NO_SUCH_FIELD) {
    return {};
  }
  require_success(event_error, "could not read websocket event type");

  std::string_view event_type;
  require_success(
      event_value.get_string().get(event_type),
      "websocket event field 'ev' must be a string");
  return std::string(event_type);
}

#endif

}  // namespace websocket_detail

WebSocketEvent::WebSocketEvent(
    std::shared_ptr<websocket_detail::EventState> state)
    : state_(std::move(state)) {
  if (!state_) {
    throw std::invalid_argument("websocket event state cannot be null");
  }
}

WebSocketEvent::WebSocketEvent(const WebSocketEvent& other) = default;
WebSocketEvent::WebSocketEvent(WebSocketEvent&& other) noexcept = default;
WebSocketEvent& WebSocketEvent::operator=(const WebSocketEvent& other) = default;
WebSocketEvent& WebSocketEvent::operator=(WebSocketEvent&& other) noexcept = default;
WebSocketEvent::~WebSocketEvent() = default;

nb::object WebSocketEvent::event_type_object() const {
  return state_->required_field("ev");
}

nb::object WebSocketEvent::get_item(std::string_view key) const {
  return state_->required_field(key);
}

nb::object WebSocketEvent::get(
    std::string_view key,
    nb::handle default_value) const {
  return state_->optional_field(key, default_value);
}

bool WebSocketEvent::contains(std::string_view key) const {
  return state_->contains(key);
}

bool WebSocketEvent::is_cached(std::string_view key) const {
  return state_->is_cached(key);
}

bool WebSocketEvent::is_property_cached(std::string_view key) const {
  return state_->is_property_cached(key);
}

nb::tuple WebSocketEvent::cached_fields() const {
  return state_->cached_fields();
}

nb::tuple WebSocketEvent::cached_properties() const {
  return state_->cached_properties();
}

nb::bytes WebSocketEvent::raw_json() const {
  const std::string_view raw = state_->raw_json_view();
  return nb::bytes(raw.data(), raw.size());
}

nb::bytes WebSocketEvent::message_bytes() const {
  const std::string_view raw = state_->message_view();
  return nb::bytes(raw.data(), raw.size());
}

std::string WebSocketEvent::repr() const {
  std::ostringstream output;
  output << "WebSocketEvent(ev='" << state_->event_type() << "')";
  return output.str();
}

const std::shared_ptr<websocket_detail::EventState>& WebSocketEvent::state() const {
  return state_;
}

nb::object WebSocketEvent::cached_property(
    std::string_view key,
    const std::function<nb::object()>& factory) const {
  return state_->cached_property(key, factory);
}

namespace {

std::string python_string(
    nb::handle value,
    std::string_view field_name) {
  if (!PyUnicode_Check(value.ptr())) {
    std::ostringstream message;
    message << "websocket field '" << field_name << "' must be a string";
    throw std::invalid_argument(message.str());
  }
  Py_ssize_t size = 0;
  const char* data = PyUnicode_AsUTF8AndSize(value.ptr(), &size);
  if (data == nullptr) {
    throw nb::python_error();
  }
  return {data, static_cast<std::size_t>(size)};
}

std::uint64_t python_unsigned(
    nb::handle value,
    std::string_view field_name) {
  if (PyLong_Check(value.ptr()) && !PyBool_Check(value.ptr())) {
    const unsigned long long parsed = PyLong_AsUnsignedLongLong(value.ptr());
    if (!PyErr_Occurred()) {
      return static_cast<std::uint64_t>(parsed);
    }
    PyErr_Clear();
  } else if (PyUnicode_Check(value.ptr())) {
    const std::string text = python_string(value, field_name);
    std::uint64_t parsed = 0;
    const auto [end, error] = std::from_chars(
        text.data(),
        text.data() + text.size(),
        parsed);
    if (error == std::errc() && end == text.data() + text.size()) {
      return parsed;
    }
  }

  std::ostringstream message;
  message << "websocket field '" << field_name
          << "' must be a non-negative integer";
  throw std::invalid_argument(message.str());
}

double python_double(
    nb::handle value,
    std::string_view field_name) {
  if ((PyFloat_Check(value.ptr()) || PyLong_Check(value.ptr())) &&
      !PyBool_Check(value.ptr())) {
    const double parsed = PyFloat_AsDouble(value.ptr());
    if (!PyErr_Occurred() && std::isfinite(parsed)) {
      return parsed;
    }
    PyErr_Clear();
  }

  std::ostringstream message;
  message << "websocket field '" << field_name
          << "' must be a finite number";
  throw std::invalid_argument(message.str());
}

std::uint64_t milliseconds_to_nanoseconds(
    std::uint64_t milliseconds,
    std::string_view field_name) {
  constexpr std::uint64_t scale = 1'000'000ULL;
  if (milliseconds > std::numeric_limits<std::uint64_t>::max() / scale) {
    std::ostringstream message;
    message << "websocket timestamp field '" << field_name << "' is out of range";
    throw std::out_of_range(message.str());
  }
  return milliseconds * scale;
}

nb::object required_uint_object(
    const WebSocketEvent& event,
    std::string_view key) {
  return nb::int_(python_unsigned(event.get_item(key), key));
}

nb::object optional_uint_object(
    const WebSocketEvent& event,
    std::string_view key,
    std::uint64_t default_value = 0) {
  nb::object raw = event.get(key, nb::none());
  if (raw.is_none()) {
    return nb::int_(default_value);
  }
  return nb::int_(python_unsigned(raw, key));
}

nb::object required_double_object(
    const WebSocketEvent& event,
    std::string_view key) {
  return nb::float_(python_double(event.get_item(key), key));
}

nb::object wire_timestamp_object(
    const WebSocketEvent& event,
    std::string_view key) {
  return nb::int_(milliseconds_to_nanoseconds(
      python_unsigned(event.get_item(key), key),
      key));
}

nb::object optional_timestamp_object(
    const WebSocketEvent& event,
    std::string_view key,
    std::uint64_t default_value = 0) {
  nb::object raw = event.get(key, nb::none());
  if (raw.is_none()) {
    return nb::int_(default_value);
  }
  return nb::int_(milliseconds_to_nanoseconds(
      python_unsigned(raw, key),
      key));
}

nb::object frozenset_object(nb::handle raw) {
  PyObject* result = nullptr;
  if (raw.is_none()) {
    result = PyFrozenSet_New(nullptr);
  } else if (PyLong_Check(raw.ptr()) && !PyBool_Check(raw.ptr())) {
    PyObject* values = PySet_New(nullptr);
    if (values == nullptr) {
      throw nb::python_error();
    }
    if (PySet_Add(values, raw.ptr()) != 0) {
      Py_DECREF(values);
      throw nb::python_error();
    }
    result = PyFrozenSet_New(values);
    Py_DECREF(values);
  } else {
    result = PyFrozenSet_New(raw.ptr());
  }
  if (result == nullptr) {
    throw nb::python_error();
  }
  return nb::steal<nb::object>(result);
}

std::bitset<96> condition_bits(nb::handle conditions) {
  std::bitset<96> bits;
  PyObject* iterator_pointer = PyObject_GetIter(conditions.ptr());
  if (iterator_pointer == nullptr) {
    throw nb::python_error();
  }
  nb::object iterator = nb::steal<nb::object>(iterator_pointer);
  while (PyObject* item_pointer = PyIter_Next(iterator.ptr())) {
    nb::object item = nb::steal<nb::object>(item_pointer);
    const std::uint64_t code = python_unsigned(item, "conditions");
    if (code < bits.size()) {
      bits.set(static_cast<std::size_t>(code));
    }
  }
  if (PyErr_Occurred()) {
    throw nb::python_error();
  }
  return bits;
}

std::string normalized_ticker(
    nb::handle raw,
    std::string_view field_name,
    std::string_view prefix,
    bool replace_slash = false) {
  std::string ticker = python_string(raw, field_name);
  if (replace_slash) {
    std::replace(ticker.begin(), ticker.end(), '/', '-');
  }
  if (!ticker.starts_with(prefix)) {
    ticker.insert(0, prefix);
  }
  return ticker;
}

struct OptionParts {
  std::string root;
  std::string expiration;
  char right = '\0';
  double strike = 0.0;
};

std::uint32_t fixed_digits(
    std::string_view text,
    std::string_view field_name) {
  std::uint32_t value = 0;
  if (text.empty()) {
    throw std::invalid_argument("empty option symbol component");
  }
  for (const char character : text) {
    if (character < '0' || character > '9') {
      std::ostringstream message;
      message << "invalid option " << field_name << ": " << text;
      throw std::invalid_argument(message.str());
    }
    value = value * 10U + static_cast<std::uint32_t>(character - '0');
  }
  return value;
}

OptionParts parse_option_parts(std::string_view ticker) {
  if (!ticker.starts_with("O:")) {
    throw std::invalid_argument("option websocket ticker must start with O:");
  }
  const std::string_view body = ticker.substr(2);
  constexpr std::size_t suffix_size = 15;
  if (body.size() <= suffix_size) {
    throw std::invalid_argument("option websocket ticker is too short");
  }
  const std::size_t suffix_start = body.size() - suffix_size;
  const std::string_view expiration = body.substr(suffix_start, 6);
  const char right = body[suffix_start + 6];
  if (right != 'C' && right != 'P') {
    throw std::invalid_argument("option websocket ticker right must be C or P");
  }
  const std::uint32_t year =
      2000U + fixed_digits(expiration.substr(0, 2), "expiration year");
  const std::uint32_t month =
      fixed_digits(expiration.substr(2, 2), "expiration month");
  const std::uint32_t day =
      fixed_digits(expiration.substr(4, 2), "expiration day");
  const std::chrono::year_month_day date{
      std::chrono::year(static_cast<int>(year)) /
      std::chrono::month(month) /
      std::chrono::day(day)};
  if (!date.ok()) {
    throw std::invalid_argument("option websocket ticker has an invalid expiration");
  }
  const std::uint32_t strike_millis =
      fixed_digits(body.substr(suffix_start + 7, 8), "strike");

  OptionParts result;
  result.root.assign(body.substr(0, suffix_start));
  result.expiration.reserve(10);
  result.expiration.append("20");
  result.expiration.append(expiration.substr(0, 2));
  result.expiration.push_back('-');
  result.expiration.append(expiration.substr(2, 2));
  result.expiration.push_back('-');
  result.expiration.append(expiration.substr(4, 2));
  result.right = right;
  result.strike = static_cast<double>(strike_millis) / 1000.0;
  return result;
}

OptionParts option_parts(const WebSocketEvent& event) {
  return parse_option_parts(python_string(event.get_item("sym"), "sym"));
}

struct DecimalQuantity {
  std::uint64_t coefficient = 0;
  std::uint8_t scale = 0;
};

DecimalQuantity parse_decimal_quantity(std::string_view text) {
  if (text.empty()) {
    return {};
  }
  std::size_t position = text.front() == '+' ? 1 : 0;
  if (text.front() == '-') {
    throw std::invalid_argument("websocket trade size cannot be negative");
  }
  DecimalQuantity result;
  bool saw_digit = false;
  bool saw_decimal_point = false;
  std::size_t scale = 0;
  constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
  for (; position < text.size(); ++position) {
    const char character = text[position];
    if (character == '.' && !saw_decimal_point) {
      saw_decimal_point = true;
      continue;
    }
    if (character < '0' || character > '9') {
      throw std::invalid_argument("invalid websocket decimal trade size");
    }
    saw_digit = true;
    const std::uint64_t digit = static_cast<std::uint64_t>(character - '0');
    if (result.coefficient > (maximum - digit) / 10U) {
      throw std::out_of_range("websocket decimal trade size is out of range");
    }
    result.coefficient = result.coefficient * 10U + digit;
    if (saw_decimal_point) {
      ++scale;
      if (scale > std::numeric_limits<std::uint8_t>::max()) {
        throw std::out_of_range("websocket decimal trade size scale is out of range");
      }
    }
  }
  if (!saw_digit) {
    throw std::invalid_argument("invalid websocket decimal trade size");
  }
  while (scale > 0 && result.coefficient % 10U == 0) {
    result.coefficient /= 10U;
    --scale;
  }
  if (result.coefficient == 0) {
    scale = 0;
  }
  result.scale = static_cast<std::uint8_t>(scale);
  return result;
}

double decimal_quantity_to_double(const DecimalQuantity& quantity) {
  return static_cast<double>(quantity.coefficient) *
      std::pow(10.0, -static_cast<int>(quantity.scale));
}

std::string decimal_quantity_string(const DecimalQuantity& quantity) {
  std::string digits = std::to_string(quantity.coefficient);
  if (quantity.scale == 0) {
    return digits;
  }
  const std::size_t scale = quantity.scale;
  if (digits.size() <= scale) {
    std::string result = "0.";
    result.append(scale - digits.size(), '0');
    result += digits;
    return result;
  }
  digits.insert(digits.size() - scale, 1, '.');
  return digits;
}

DecimalQuantity stock_trade_quantity(
    const WebSocketStockTrade& event) {
  nb::object raw = event.get("ds", nb::none());
  std::string text;
  if (!raw.is_none()) {
    text = python_string(raw, "ds");
  } else {
    nb::object size = event.get_item("s");
    PyObject* rendered = PyObject_Str(size.ptr());
    if (rendered == nullptr) {
      throw nb::python_error();
    }
    nb::object rendered_object = nb::steal<nb::object>(rendered);
    text = python_string(rendered_object, "s");
  }
  return parse_decimal_quantity(text);
}

bool none_of(
    const std::bitset<96>& bits,
    std::initializer_list<std::size_t> excluded) {
  return std::none_of(
      excluded.begin(),
      excluded.end(),
      [&bits](std::size_t condition) { return bits.test(condition); });
}

nb::object currency_tickers_object(std::string_view ticker) {
  const std::size_t colon = ticker.find(':');
  const std::string_view pair =
      colon == std::string_view::npos ? ticker : ticker.substr(colon + 1);
  const std::size_t dash = pair.find('-');
  const std::string_view base =
      dash == std::string_view::npos ? pair : pair.substr(0, dash);
  const std::string_view quote =
      dash == std::string_view::npos ? std::string_view{} : pair.substr(dash + 1);
  nb::tuple result = nb::steal<nb::tuple>(PyTuple_New(2));
  if (!result.is_valid()) {
    throw nb::python_error();
  }
  PyTuple_SET_ITEM(
      result.ptr(),
      0,
      nb::str(base.data(), base.size()).release().ptr());
  PyTuple_SET_ITEM(
      result.ptr(),
      1,
      nb::str(quote.data(), quote.size()).release().ptr());
  return result;
}

nb::object constant_zero() { return nb::int_(0); }

nb::object constant_empty_string() { return nb::str(""); }

}  // namespace

#define MASSIVE_SPEEDUP_WS_DIRECT(class_, property_, source_) \
  nb::object class_::property_##_object() const { \
    return cached_property(#property_, [this] { return get_item(source_); }); \
  }

#define MASSIVE_SPEEDUP_WS_UINT(class_, property_, source_) \
  nb::object class_::property_##_object() const { \
    return cached_property( \
        #property_, \
        [this] { return required_uint_object(*this, source_); }); \
  }

#define MASSIVE_SPEEDUP_WS_DOUBLE(class_, property_, source_) \
  nb::object class_::property_##_object() const { \
    return cached_property( \
        #property_, \
        [this] { return required_double_object(*this, source_); }); \
  }

#define MASSIVE_SPEEDUP_WS_TIMESTAMP(class_, property_, source_) \
  nb::object class_::property_##_object() const { \
    return cached_property( \
        #property_, \
        [this] { return wire_timestamp_object(*this, source_); }); \
  }

#define MASSIVE_SPEEDUP_WS_ZERO(class_, property_) \
  nb::object class_::property_##_object() const { \
    return cached_property(#property_, &constant_zero); \
  }

#define MASSIVE_SPEEDUP_WS_EMPTY_STRING(class_, property_) \
  nb::object class_::property_##_object() const { \
    return cached_property(#property_, &constant_empty_string); \
  }

nb::object WebSocketStatus::status_object() const {
  return cached_property("status", [this] {
    nb::object value = get("status", nb::none());
    return value.is_none() ? nb::object(nb::str("")) : value;
  });
}

nb::object WebSocketStatus::message_object() const {
  return cached_property("message", [this] {
    nb::object value = get("message", nb::none());
    return value.is_none() ? nb::object(nb::str("")) : value;
  });
}

MASSIVE_SPEEDUP_WS_DIRECT(WebSocketStockTrade, ticker, "sym")

nb::object WebSocketStockTrade::conditions_object() const {
  return cached_property("conditions", [this] {
    return frozenset_object(get("c", nb::none()));
  });
}

MASSIVE_SPEEDUP_WS_ZERO(WebSocketStockTrade, correction)
MASSIVE_SPEEDUP_WS_UINT(WebSocketStockTrade, exchange, "x")
MASSIVE_SPEEDUP_WS_UINT(WebSocketStockTrade, id, "i")

nb::object WebSocketStockTrade::participant_timestamp_object() const {
  return cached_property("participant_timestamp", [this] {
    nb::object participant = get("pt", nb::none());
    if (participant.is_none()) {
      return wire_timestamp_object(*this, "t");
    }
    return nb::object(nb::int_(milliseconds_to_nanoseconds(
        python_unsigned(participant, "pt"),
        "pt")));
  });
}

MASSIVE_SPEEDUP_WS_DOUBLE(WebSocketStockTrade, price, "p")
MASSIVE_SPEEDUP_WS_UINT(WebSocketStockTrade, sequence_number, "q")
MASSIVE_SPEEDUP_WS_TIMESTAMP(WebSocketStockTrade, sip_timestamp, "t")

nb::object WebSocketStockTrade::decimal_size_object() const {
  return cached_property("decimal_size", [this] {
    const auto quantity = stock_trade_quantity(*this);
    const std::string text = decimal_quantity_string(quantity);
    return nb::object(nb::str(text.data(), text.size()));
  });
}

nb::object WebSocketStockTrade::size_object() const {
  return cached_property("size", [this] {
    const auto quantity = stock_trade_quantity(*this);
    return nb::object(nb::float_(
        decimal_quantity_to_double(quantity)));
  });
}

nb::object WebSocketStockTrade::size_coefficient_object() const {
  return cached_property("size_coefficient", [this] {
    return nb::object(nb::int_(stock_trade_quantity(*this).coefficient));
  });
}

nb::object WebSocketStockTrade::size_scale_object() const {
  return cached_property("size_scale", [this] {
    return nb::object(nb::int_(stock_trade_quantity(*this).scale));
  });
}

MASSIVE_SPEEDUP_WS_UINT(WebSocketStockTrade, tape, "z")

nb::object WebSocketStockTrade::trf_id_object() const {
  return cached_property("trf_id", [this] {
    return optional_uint_object(*this, "trfi");
  });
}

nb::object WebSocketStockTrade::trf_timestamp_object() const {
  return cached_property("trf_timestamp", [this] {
    return optional_timestamp_object(*this, "trft");
  });
}

bool WebSocketStockTrade::updates_high_low() const {
  return none_of(
      condition_bits(conditions_object()),
      {2, 7, 12, 13, 15, 16, 20, 21, 29, 37, 52, 53});
}

bool WebSocketStockTrade::updates_open_close() const {
  return none_of(
      condition_bits(conditions_object()),
      {2, 5, 7, 10, 12, 13, 15, 16, 20, 21, 22, 29, 32, 33, 37, 52, 53});
}

bool WebSocketStockTrade::updates_volume() const {
  return none_of(condition_bits(conditions_object()), {15, 16, 38});
}

MASSIVE_SPEEDUP_WS_DIRECT(WebSocketStockQuote, ticker, "sym")
MASSIVE_SPEEDUP_WS_UINT(WebSocketStockQuote, ask_exchange, "ax")
MASSIVE_SPEEDUP_WS_DOUBLE(WebSocketStockQuote, ask_price, "ap")
MASSIVE_SPEEDUP_WS_UINT(WebSocketStockQuote, ask_size, "as")
MASSIVE_SPEEDUP_WS_UINT(WebSocketStockQuote, bid_exchange, "bx")
MASSIVE_SPEEDUP_WS_DOUBLE(WebSocketStockQuote, bid_price, "bp")
MASSIVE_SPEEDUP_WS_UINT(WebSocketStockQuote, bid_size, "bs")

nb::object WebSocketStockQuote::conditions_object() const {
  return cached_property("conditions", [this] {
    return frozenset_object(get("c", nb::none()));
  });
}

nb::object WebSocketStockQuote::indicators_object() const {
  return cached_property("indicators", [this] {
    return frozenset_object(get("i", nb::none()));
  });
}

MASSIVE_SPEEDUP_WS_TIMESTAMP(
    WebSocketStockQuote,
    participant_timestamp,
    "t")
MASSIVE_SPEEDUP_WS_UINT(WebSocketStockQuote, sequence_number, "q")
MASSIVE_SPEEDUP_WS_TIMESTAMP(WebSocketStockQuote, sip_timestamp, "t")
MASSIVE_SPEEDUP_WS_UINT(WebSocketStockQuote, tape, "z")
MASSIVE_SPEEDUP_WS_ZERO(WebSocketStockQuote, trf_timestamp)

bool WebSocketStockQuote::updates_high_low() const {
  return true;
}

bool WebSocketStockQuote::updates_open_close() const {
  return true;
}

bool WebSocketStockQuote::updates_volume() const {
  return true;
}

MASSIVE_SPEEDUP_WS_DIRECT(WebSocketOptionTrade, ticker, "sym")

nb::object WebSocketOptionTrade::root_object() const {
  return cached_property("root", [this] {
    const std::string& root = option_parts(*this).root;
    return nb::object(nb::str(root.data(), root.size()));
  });
}

nb::object WebSocketOptionTrade::expiration_object() const {
  return cached_property("expiration", [this] {
    const std::string& expiration = option_parts(*this).expiration;
    return nb::object(nb::str(expiration.data(), expiration.size()));
  });
}

nb::object WebSocketOptionTrade::right_object() const {
  return cached_property("right", [this] {
    const char right = option_parts(*this).right;
    return nb::object(nb::str(&right, 1));
  });
}

nb::object WebSocketOptionTrade::strike_object() const {
  return cached_property("strike", [this] {
    return nb::object(nb::float_(option_parts(*this).strike));
  });
}

nb::object WebSocketOptionTrade::conditions_object() const {
  return cached_property("conditions", [this] {
    return frozenset_object(get("c", nb::none()));
  });
}

MASSIVE_SPEEDUP_WS_ZERO(WebSocketOptionTrade, correction)
MASSIVE_SPEEDUP_WS_UINT(WebSocketOptionTrade, exchange, "x")
MASSIVE_SPEEDUP_WS_DOUBLE(WebSocketOptionTrade, price, "p")
MASSIVE_SPEEDUP_WS_UINT(WebSocketOptionTrade, sequence_number, "q")
MASSIVE_SPEEDUP_WS_TIMESTAMP(WebSocketOptionTrade, sip_timestamp, "t")
MASSIVE_SPEEDUP_WS_UINT(WebSocketOptionTrade, size, "s")

MASSIVE_SPEEDUP_WS_DIRECT(WebSocketOptionQuote, ticker, "sym")

nb::object WebSocketOptionQuote::root_object() const {
  return cached_property("root", [this] {
    const std::string& root = option_parts(*this).root;
    return nb::object(nb::str(root.data(), root.size()));
  });
}

nb::object WebSocketOptionQuote::expiration_object() const {
  return cached_property("expiration", [this] {
    const std::string& expiration = option_parts(*this).expiration;
    return nb::object(nb::str(expiration.data(), expiration.size()));
  });
}

nb::object WebSocketOptionQuote::right_object() const {
  return cached_property("right", [this] {
    const char right = option_parts(*this).right;
    return nb::object(nb::str(&right, 1));
  });
}

nb::object WebSocketOptionQuote::strike_object() const {
  return cached_property("strike", [this] {
    return nb::object(nb::float_(option_parts(*this).strike));
  });
}

MASSIVE_SPEEDUP_WS_UINT(WebSocketOptionQuote, ask_exchange, "ax")
MASSIVE_SPEEDUP_WS_DOUBLE(WebSocketOptionQuote, ask_price, "ap")
MASSIVE_SPEEDUP_WS_UINT(WebSocketOptionQuote, ask_size, "as")
MASSIVE_SPEEDUP_WS_UINT(WebSocketOptionQuote, bid_exchange, "bx")
MASSIVE_SPEEDUP_WS_DOUBLE(WebSocketOptionQuote, bid_price, "bp")
MASSIVE_SPEEDUP_WS_UINT(WebSocketOptionQuote, bid_size, "bs")
MASSIVE_SPEEDUP_WS_UINT(WebSocketOptionQuote, sequence_number, "q")
MASSIVE_SPEEDUP_WS_TIMESTAMP(WebSocketOptionQuote, sip_timestamp, "t")

MASSIVE_SPEEDUP_WS_DIRECT(WebSocketFuturesTrade, ticker, "sym")
MASSIVE_SPEEDUP_WS_TIMESTAMP(WebSocketFuturesTrade, timestamp, "t")
MASSIVE_SPEEDUP_WS_UINT(WebSocketFuturesTrade, sequence_number, "q")
MASSIVE_SPEEDUP_WS_ZERO(WebSocketFuturesTrade, report_sequence)
MASSIVE_SPEEDUP_WS_DOUBLE(WebSocketFuturesTrade, price, "p")
MASSIVE_SPEEDUP_WS_UINT(WebSocketFuturesTrade, size, "s")
MASSIVE_SPEEDUP_WS_ZERO(WebSocketFuturesTrade, correction)
MASSIVE_SPEEDUP_WS_ZERO(WebSocketFuturesTrade, exchange)
MASSIVE_SPEEDUP_WS_EMPTY_STRING(WebSocketFuturesTrade, session_end_date)

MASSIVE_SPEEDUP_WS_DIRECT(WebSocketFuturesQuote, ticker, "sym")
MASSIVE_SPEEDUP_WS_TIMESTAMP(WebSocketFuturesQuote, timestamp, "t")
MASSIVE_SPEEDUP_WS_ZERO(WebSocketFuturesQuote, sequence_number)
MASSIVE_SPEEDUP_WS_ZERO(WebSocketFuturesQuote, report_sequence)
MASSIVE_SPEEDUP_WS_TIMESTAMP(WebSocketFuturesQuote, ask_timestamp, "at")
MASSIVE_SPEEDUP_WS_DOUBLE(WebSocketFuturesQuote, ask_price, "ap")
MASSIVE_SPEEDUP_WS_UINT(WebSocketFuturesQuote, ask_size, "as")
MASSIVE_SPEEDUP_WS_TIMESTAMP(WebSocketFuturesQuote, bid_timestamp, "bt")
MASSIVE_SPEEDUP_WS_DOUBLE(WebSocketFuturesQuote, bid_price, "bp")
MASSIVE_SPEEDUP_WS_UINT(WebSocketFuturesQuote, bid_size, "bs")
MASSIVE_SPEEDUP_WS_ZERO(WebSocketFuturesQuote, exchange)
MASSIVE_SPEEDUP_WS_EMPTY_STRING(WebSocketFuturesQuote, session_end_date)

nb::object WebSocketCryptoTrade::ticker_object() const {
  return cached_property("ticker", [this] {
    const std::string ticker = normalized_ticker(
        get_item("pair"),
        "pair",
        "X:");
    return nb::object(nb::str(ticker.data(), ticker.size()));
  });
}

nb::object WebSocketCryptoTrade::conditions_object() const {
  return cached_property("conditions", [this] {
    return frozenset_object(get("c", nb::none()));
  });
}

MASSIVE_SPEEDUP_WS_UINT(WebSocketCryptoTrade, exchange, "x")

nb::object WebSocketCryptoTrade::id_object() const {
  return cached_property("id", [this] {
    return optional_uint_object(*this, "i");
  });
}

MASSIVE_SPEEDUP_WS_TIMESTAMP(
    WebSocketCryptoTrade,
    participant_timestamp,
    "t")
MASSIVE_SPEEDUP_WS_TIMESTAMP(WebSocketCryptoTrade, received_timestamp, "r")
MASSIVE_SPEEDUP_WS_DOUBLE(WebSocketCryptoTrade, price, "p")
MASSIVE_SPEEDUP_WS_DOUBLE(WebSocketCryptoTrade, size, "s")

nb::object WebSocketCryptoQuote::ticker_object() const {
  return cached_property("ticker", [this] {
    const std::string ticker = normalized_ticker(
        get_item("pair"),
        "pair",
        "X:");
    return nb::object(nb::str(ticker.data(), ticker.size()));
  });
}

MASSIVE_SPEEDUP_WS_DOUBLE(WebSocketCryptoQuote, ask_price, "ap")
MASSIVE_SPEEDUP_WS_DOUBLE(WebSocketCryptoQuote, ask_size, "as")
MASSIVE_SPEEDUP_WS_DOUBLE(WebSocketCryptoQuote, bid_price, "bp")
MASSIVE_SPEEDUP_WS_DOUBLE(WebSocketCryptoQuote, bid_size, "bs")
MASSIVE_SPEEDUP_WS_UINT(WebSocketCryptoQuote, exchange, "x")
MASSIVE_SPEEDUP_WS_TIMESTAMP(
    WebSocketCryptoQuote,
    participant_timestamp,
    "t")
MASSIVE_SPEEDUP_WS_TIMESTAMP(WebSocketCryptoQuote, received_timestamp, "r")

nb::object WebSocketCurrencyQuote::ticker_object() const {
  return cached_property("ticker", [this] {
    const std::string ticker = normalized_ticker(
        get_item("p"),
        "p",
        "C:",
        true);
    return nb::object(nb::str(ticker.data(), ticker.size()));
  });
}

nb::object WebSocketCurrencyQuote::tickers_object() const {
  return cached_property("tickers", [this] {
    return currency_tickers_object(python_string(ticker_object(), "ticker"));
  });
}

nb::object WebSocketCurrencyQuote::ask_exchange_object() const {
  return cached_property("ask_exchange", [this] {
    return required_uint_object(*this, "x");
  });
}

MASSIVE_SPEEDUP_WS_DOUBLE(WebSocketCurrencyQuote, ask_price, "a")

nb::object WebSocketCurrencyQuote::bid_exchange_object() const {
  return cached_property("bid_exchange", [this] {
    return required_uint_object(*this, "x");
  });
}

MASSIVE_SPEEDUP_WS_DOUBLE(WebSocketCurrencyQuote, bid_price, "b")
MASSIVE_SPEEDUP_WS_TIMESTAMP(
    WebSocketCurrencyQuote,
    participant_timestamp,
    "t")

nb::object WebSocketIndexValue::ticker_object() const {
  return cached_property("ticker", [this] {
    const std::string ticker = normalized_ticker(
        get_item("T"),
        "T",
        "I:");
    return nb::object(nb::str(ticker.data(), ticker.size()));
  });
}

MASSIVE_SPEEDUP_WS_DOUBLE(WebSocketIndexValue, value, "val")
MASSIVE_SPEEDUP_WS_TIMESTAMP(WebSocketIndexValue, timestamp, "t")

#undef MASSIVE_SPEEDUP_WS_DIRECT
#undef MASSIVE_SPEEDUP_WS_UINT
#undef MASSIVE_SPEEDUP_WS_DOUBLE
#undef MASSIVE_SPEEDUP_WS_TIMESTAMP
#undef MASSIVE_SPEEDUP_WS_ZERO
#undef MASSIVE_SPEEDUP_WS_EMPTY_STRING

WebSocketMessage::WebSocketMessage(
    std::shared_ptr<websocket_detail::MessageState> state,
    nb::tuple events)
    : state_(std::move(state)), events_(std::move(events)) {
  if (!state_) {
    throw std::invalid_argument("websocket message state cannot be null");
  }
}

WebSocketMessage::WebSocketMessage(const WebSocketMessage& other) = default;
WebSocketMessage::WebSocketMessage(WebSocketMessage&& other) noexcept = default;
WebSocketMessage& WebSocketMessage::operator=(const WebSocketMessage& other) = default;
WebSocketMessage& WebSocketMessage::operator=(WebSocketMessage&& other) noexcept = default;
WebSocketMessage::~WebSocketMessage() = default;

std::size_t WebSocketMessage::size() const {
  return static_cast<std::size_t>(PyTuple_GET_SIZE(events_.ptr()));
}

nb::object WebSocketMessage::get_item(nb::handle index) const {
  PyObject* value = PyObject_GetItem(events_.ptr(), index.ptr());
  if (value == nullptr) {
    throw nb::python_error();
  }
  return nb::steal<nb::object>(value);
}

nb::object WebSocketMessage::iterator() const {
  PyObject* value = PyObject_GetIter(events_.ptr());
  if (value == nullptr) {
    throw nb::python_error();
  }
  return nb::steal<nb::object>(value);
}

nb::tuple WebSocketMessage::events() const { return events_; }

nb::bytes WebSocketMessage::raw_json() const {
  const std::string_view raw = state_->bytes();
  return nb::bytes(raw.data(), raw.size());
}

std::string WebSocketMessage::asset_class() const {
  return websocket_asset_name(state_->asset());
}

std::string WebSocketMessage::repr() const {
  std::ostringstream output;
  output << "WebSocketMessage(asset_class='" << asset_class()
         << "', events=" << size() << ")";
  return output.str();
}

namespace {

nb::object typed_event_object(
    WebSocketAsset asset,
    std::string_view event_type,
    std::shared_ptr<websocket_detail::EventState> state) {
  if (event_type == "status") {
    return nb::cast(WebSocketStatus(std::move(state)));
  }

  switch (asset) {
    case WebSocketAsset::Stocks:
      if (event_type == "T") {
        return nb::cast(WebSocketStockTrade(std::move(state)));
      }
      if (event_type == "Q") {
        return nb::cast(WebSocketStockQuote(std::move(state)));
      }
      break;
    case WebSocketAsset::Options:
      if (event_type == "T") {
        return nb::cast(WebSocketOptionTrade(std::move(state)));
      }
      if (event_type == "Q") {
        return nb::cast(WebSocketOptionQuote(std::move(state)));
      }
      break;
    case WebSocketAsset::Futures:
      if (event_type == "T") {
        return nb::cast(WebSocketFuturesTrade(std::move(state)));
      }
      if (event_type == "Q") {
        return nb::cast(WebSocketFuturesQuote(std::move(state)));
      }
      break;
    case WebSocketAsset::Indices:
      if (event_type == "V") {
        return nb::cast(WebSocketIndexValue(std::move(state)));
      }
      break;
    case WebSocketAsset::Forex:
      if (event_type == "C") {
        return nb::cast(WebSocketCurrencyQuote(std::move(state)));
      }
      break;
    case WebSocketAsset::Crypto:
      if (event_type == "XT") {
        return nb::cast(WebSocketCryptoTrade(std::move(state)));
      }
      if (event_type == "XQ") {
        return nb::cast(WebSocketCryptoQuote(std::move(state)));
      }
      break;
    case WebSocketAsset::Messages:
      break;
  }
  return nb::cast(WebSocketEvent(std::move(state)));
}

}  // namespace

WebSocketMessage parse_websocket_message(
    nb::handle payload,
    WebSocketAsset asset) {
  const std::string materialized = payload_to_string(payload);
  if (materialized.empty()) {
    throw std::invalid_argument("websocket message cannot be empty");
  }

  auto message = std::make_shared<websocket_detail::MessageState>(
      materialized,
      asset);

#if MASSIVE_SPEEDUP_HAS_SIMDJSON
  const std::vector<websocket_detail::EventSlice> slices =
      websocket_detail::find_event_slices(*message);
  nb::tuple events = nb::steal<nb::tuple>(
      PyTuple_New(static_cast<Py_ssize_t>(slices.size())));
  if (!events.is_valid()) {
    throw nb::python_error();
  }
  simdjson::ondemand::parser classifier;
  for (std::size_t index = 0; index < slices.size(); ++index) {
    const auto& slice = slices[index];
    auto state = std::make_shared<websocket_detail::EventState>(
        message,
        slice.offset,
        slice.length,
        websocket_detail::classify_event(classifier, *message, slice));
    const std::string event_type = state->event_type();
    PyTuple_SET_ITEM(
        events.ptr(),
        static_cast<Py_ssize_t>(index),
        typed_event_object(asset, event_type, std::move(state)).release().ptr());
  }
  return WebSocketMessage(std::move(message), std::move(events));
#else
  throw std::runtime_error(
      "websocket parsing requires a build with simdjson support");
#endif
}

const char* websocket_asset_name(WebSocketAsset asset) noexcept {
  switch (asset) {
    case WebSocketAsset::Messages:
      return "messages";
    case WebSocketAsset::Stocks:
      return "stocks";
    case WebSocketAsset::Options:
      return "options";
    case WebSocketAsset::Futures:
      return "futures";
    case WebSocketAsset::Indices:
      return "indices";
    case WebSocketAsset::Forex:
      return "forex";
    case WebSocketAsset::Crypto:
      return "crypto";
  }
  return "messages";
}

}  // namespace massive_speedup::native
