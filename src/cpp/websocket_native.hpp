#pragma once

#include <nanobind/nanobind.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace massive_speedup::native {

namespace nb = nanobind;

enum class WebSocketAsset : std::uint8_t {
  Messages,
  Stocks,
  Options,
  Futures,
  Indices,
  Forex,
  Crypto,
};

namespace websocket_detail {

class MessageState;
class EventState;

}  // namespace websocket_detail

class WebSocketEvent {
 public:
  explicit WebSocketEvent(
      std::shared_ptr<websocket_detail::EventState> state);
  WebSocketEvent(const WebSocketEvent& other);
  WebSocketEvent(WebSocketEvent&& other) noexcept;
  WebSocketEvent& operator=(const WebSocketEvent& other);
  WebSocketEvent& operator=(WebSocketEvent&& other) noexcept;
  virtual ~WebSocketEvent();

  nb::object event_type_object() const;
  nb::object get_item(std::string_view key) const;
  nb::object get(std::string_view key, nb::handle default_value) const;
  bool contains(std::string_view key) const;
  bool is_cached(std::string_view key) const;
  nb::tuple cached_fields() const;
  nb::bytes raw_json() const;
  nb::bytes message_bytes() const;
  std::string repr() const;

 protected:
  const std::shared_ptr<websocket_detail::EventState>& state() const;

 private:
  std::shared_ptr<websocket_detail::EventState> state_;
};

class WebSocketMessage {
 public:
  WebSocketMessage(
      std::shared_ptr<websocket_detail::MessageState> state,
      nb::tuple events);
  WebSocketMessage(const WebSocketMessage& other);
  WebSocketMessage(WebSocketMessage&& other) noexcept;
  WebSocketMessage& operator=(const WebSocketMessage& other);
  WebSocketMessage& operator=(WebSocketMessage&& other) noexcept;
  ~WebSocketMessage();

  std::size_t size() const;
  nb::object get_item(nb::handle index) const;
  nb::object iterator() const;
  nb::tuple events() const;
  nb::bytes raw_json() const;
  std::string asset_class() const;
  std::string repr() const;

 private:
  std::shared_ptr<websocket_detail::MessageState> state_;
  nb::tuple events_;
};

WebSocketMessage parse_websocket_message(
    nb::handle payload,
    WebSocketAsset asset);

const char* websocket_asset_name(WebSocketAsset asset) noexcept;

}  // namespace massive_speedup::native
