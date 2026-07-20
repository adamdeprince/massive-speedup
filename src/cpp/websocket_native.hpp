#pragma once

#include <nanobind/nanobind.h>

#include <cstddef>
#include <cstdint>
#include <functional>
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
class FeedState;
class MarketState;

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
  bool is_property_cached(std::string_view key) const;
  nb::tuple cached_fields() const;
  nb::tuple cached_properties() const;
  nb::bytes raw_json() const;
  nb::bytes message_bytes() const;
  std::string asset_class() const;
  WebSocketAsset asset() const noexcept;
  std::string repr() const;

 protected:
  const std::shared_ptr<websocket_detail::EventState>& state() const;
  nb::object cached_property(
      std::string_view key,
      const std::function<nb::object()>& factory) const;

 private:
  std::shared_ptr<websocket_detail::EventState> state_;
};

#define MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(name) \
  nb::object name##_object() const

class WebSocketStatus final : public WebSocketEvent {
 public:
  using WebSocketEvent::WebSocketEvent;
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(status);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(message);
};

class WebSocketFairMarketValue final : public WebSocketEvent {
 public:
  using WebSocketEvent::WebSocketEvent;
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(ticker);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(value);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(fair_market_value);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(timestamp);
};

class WebSocketStockLimitUpLimitDown final : public WebSocketEvent {
 public:
  using WebSocketEvent::WebSocketEvent;
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(ticker);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(high_price);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(low_price);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(indicators);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(tape);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(timestamp);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(sequence_number);
};

class WebSocketStockImbalance final : public WebSocketEvent {
 public:
  using WebSocketEvent::WebSocketEvent;
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(ticker);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(timestamp);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(auction_time);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(auction_type);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(sequence_number);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(exchange);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(imbalance_quantity);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(paired_quantity);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(book_clearing_price);
};

class WebSocketStockTrade final : public WebSocketEvent {
 public:
  using WebSocketEvent::WebSocketEvent;
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(ticker);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(conditions);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(correction);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(exchange);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(id);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(participant_timestamp);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(price);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(sequence_number);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(sip_timestamp);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(size);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(decimal_size);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(size_coefficient);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(size_scale);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(tape);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(trf_id);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(trf_timestamp);
  bool updates_high_low() const;
  bool updates_open_close() const;
  bool updates_volume() const;
};

class WebSocketStockQuote final : public WebSocketEvent {
 public:
  using WebSocketEvent::WebSocketEvent;
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(ticker);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(ask_exchange);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(ask_price);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(ask_size);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(bid_exchange);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(bid_price);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(bid_size);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(conditions);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(indicators);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(participant_timestamp);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(sequence_number);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(sip_timestamp);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(tape);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(trf_timestamp);
  bool updates_high_low() const;
  bool updates_open_close() const;
  bool updates_volume() const;
};

class WebSocketOptionTrade final : public WebSocketEvent {
 public:
  using WebSocketEvent::WebSocketEvent;
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(ticker);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(root);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(expiration);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(right);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(strike);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(conditions);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(correction);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(exchange);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(price);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(sequence_number);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(sip_timestamp);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(size);
};

class WebSocketOptionQuote final : public WebSocketEvent {
 public:
  using WebSocketEvent::WebSocketEvent;
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(ticker);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(root);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(expiration);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(right);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(strike);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(ask_exchange);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(ask_price);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(ask_size);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(bid_exchange);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(bid_price);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(bid_size);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(sequence_number);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(sip_timestamp);
};

class WebSocketFuturesTrade final : public WebSocketEvent {
 public:
  using WebSocketEvent::WebSocketEvent;
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(ticker);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(timestamp);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(sequence_number);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(report_sequence);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(price);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(size);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(correction);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(exchange);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(session_end_date);
};

class WebSocketFuturesQuote final : public WebSocketEvent {
 public:
  using WebSocketEvent::WebSocketEvent;
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(ticker);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(timestamp);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(sequence_number);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(report_sequence);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(ask_timestamp);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(ask_price);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(ask_size);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(bid_timestamp);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(bid_price);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(bid_size);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(exchange);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(session_end_date);
};

class WebSocketCryptoTrade final : public WebSocketEvent {
 public:
  using WebSocketEvent::WebSocketEvent;
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(ticker);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(conditions);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(exchange);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(id);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(participant_timestamp);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(received_timestamp);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(price);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(size);
};

class WebSocketCryptoQuote final : public WebSocketEvent {
 public:
  using WebSocketEvent::WebSocketEvent;
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(ticker);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(ask_price);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(ask_size);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(bid_price);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(bid_size);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(exchange);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(participant_timestamp);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(received_timestamp);
};

class WebSocketCurrencyQuote final : public WebSocketEvent {
 public:
  using WebSocketEvent::WebSocketEvent;
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(ticker);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(tickers);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(ask_exchange);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(ask_price);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(bid_exchange);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(bid_price);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(participant_timestamp);
};

class WebSocketIndexValue final : public WebSocketEvent {
 public:
  using WebSocketEvent::WebSocketEvent;
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(ticker);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(value);
  MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY(timestamp);
};

#undef MASSIVE_SPEEDUP_DECLARE_WS_PROPERTY

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
  WebSocketAsset asset() const noexcept;
  std::string repr() const;

 private:
  std::shared_ptr<websocket_detail::MessageState> state_;
  nb::tuple events_;
};

class WebSocketFeed {
 public:
  WebSocketFeed(
      WebSocketAsset asset,
      std::string subscriptions,
      std::string api_key,
      std::string url,
      double timeout_seconds,
      std::size_t queue_capacity,
      bool reconnect);
  WebSocketFeed(const WebSocketFeed& other);
  WebSocketFeed(WebSocketFeed&& other) noexcept;
  WebSocketFeed& operator=(const WebSocketFeed& other);
  WebSocketFeed& operator=(WebSocketFeed&& other) noexcept;
  ~WebSocketFeed();

  WebSocketFeed& iter();
  WebSocketMessage next();
  void close();
  bool closed() const noexcept;
  bool reconnect() const noexcept;
  std::string subscriptions() const;
  std::string url() const;
  std::string asset_class() const;

 private:
  std::shared_ptr<websocket_detail::MessageState> next_message_state();

  std::shared_ptr<websocket_detail::FeedState> state_;

  friend class websocket_detail::MarketState;
};

class WebSocketMarket {
 public:
  WebSocketMarket(
      nb::handle messages,
      nb::handle broker,
      WebSocketAsset asset,
      bool quotes,
      bool fast);
  WebSocketMarket(const WebSocketMarket& other);
  WebSocketMarket(WebSocketMarket&& other) noexcept;
  WebSocketMarket& operator=(const WebSocketMarket& other);
  WebSocketMarket& operator=(WebSocketMarket&& other) noexcept;
  ~WebSocketMarket();

  WebSocketMarket& iter();
  nb::tuple next();
  nb::object broker() const;
  bool quotes() const noexcept;
  bool fast() const noexcept;
  std::string asset_class() const;

 private:
  std::shared_ptr<websocket_detail::MarketState> state_;
};

WebSocketMessage parse_websocket_message(
    nb::handle payload,
    WebSocketAsset asset);

std::string default_websocket_url(WebSocketAsset asset);
const char* websocket_asset_name(WebSocketAsset asset) noexcept;

}  // namespace massive_speedup::native
