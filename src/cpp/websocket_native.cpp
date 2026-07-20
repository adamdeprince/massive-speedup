#include "websocket_native.hpp"

#include "massive_speedup/parsers.hpp"

#include <Python.h>

#include <algorithm>
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
    std::lock_guard<std::mutex> lock(mutex_);
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
    std::lock_guard<std::mutex> lock(mutex_);
    auto value = field_locked(key);
    if (value.has_value()) {
      return *value;
    }
    return nb::borrow<nb::object>(default_value);
  }

  bool contains(std::string_view key) {
    std::lock_guard<std::mutex> lock(mutex_);
    return field_locked(key).has_value();
  }

  bool is_cached(std::string_view key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return fields_.find(std::string(key)) != fields_.end();
  }

  nb::tuple cached_fields() const {
    std::lock_guard<std::mutex> lock(mutex_);
    nb::tuple result = nb::steal<nb::tuple>(
        PyTuple_New(static_cast<Py_ssize_t>(access_order_.size())));
    if (!result.is_valid()) {
      throw nb::python_error();
    }
    for (std::size_t index = 0; index < access_order_.size(); ++index) {
      const std::string& key = access_order_[index];
      PyTuple_SET_ITEM(
          result.ptr(),
          static_cast<Py_ssize_t>(index),
          nb::str(key.data(), key.size()).release().ptr());
    }
    return result;
  }

 private:
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
  mutable std::mutex mutex_;
  std::unordered_map<std::string, nb::object> fields_;
  std::unordered_set<std::string> missing_fields_;
  std::vector<std::string> access_order_;
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

nb::tuple WebSocketEvent::cached_fields() const {
  return state_->cached_fields();
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
    PyTuple_SET_ITEM(
        events.ptr(),
        static_cast<Py_ssize_t>(index),
        nb::cast(WebSocketEvent(std::move(state))).release().ptr());
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
