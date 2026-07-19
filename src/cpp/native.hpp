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
#include <cctype>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <generator>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
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

enum class StockTradeConditionCode : std::uint8_t {
  ACQUISITION = 1,
  AVERAGE_PRICE_TRADE = 2,
  AUTOMATIC_EXECUTION = 3,
  BUNCHED_TRADE = 4,
  BUNCHED_SOLD_TRADE = 5,
  CAP_ELECTION = 6,
  CASH_SALE = 7,
  CLOSING_PRINTS = 8,
  CROSS_TRADE = 9,
  DERIVATIVELY_PRICED = 10,
  DISTRIBUTION = 11,
  FORM_T_EXTENDED_HOURS = 12,
  EXTENDED_HOURS_SOLD_OUT_OF_SEQUENCE = 13,
  INTERMARKET_SWEEP = 14,
  MARKET_CENTER_OFFICIAL_CLOSE = 15,
  MARKET_CENTER_OFFICIAL_OPEN = 16,
  MARKET_CENTER_OPENING_TRADE = 17,
  MARKET_CENTER_REOPENING_TRADE = 18,
  MARKET_CENTER_CLOSING_TRADE = 19,
  NEXT_DAY = 20,
  PRICE_VARIATION_TRADE = 21,
  PRIOR_REFERENCE_PRICE = 22,
  RULE_155_TRADE_AMEX = 23,
  RULE_127_NYSE_ONLY = 24,
  OPENING_PRINTS = 25,
  STOPPED_STOCK_REGULAR_TRADE = 27,
  RE_OPENING_PRINTS = 28,
  SELLER = 29,
  SOLD_LAST = 30,
  SOLD_LAST_AND_STOPPED_STOCK = 31,
  SOLD_OUT_OF_SEQUENCE = 32,
  SOLD_OUT_OF_SEQUENCE_AND_STOPPED_STOCK = 33,
  SPLIT_TRADE = 34,
  STOCK_OPTION = 35,
  YELLOW_FLAG_REGULAR_TRADE = 36,
  ODD_LOT_TRADE = 37,
  CORRECTED_CONSOLIDATED_CLOSE_PER_LISTING_MARKET = 38,
  TRADE_THRU_EXEMPT = 41,
  CONTINGENT_TRADE = 52,
  QUALIFIED_CONTINGENT_TRADE = 53,
  OPENING_REOPENING_TRADE_DETAIL = 55,
  SHORT_SALE_RESTRICTION_ACTIVATED = 57,
  SHORT_SALE_RESTRICTION_CONTINUED = 58,
  SHORT_SALE_RESTRICTION_DEACTIVATED = 59,
  SHORT_SALE_RESTRICTION_IN_EFFECT = 60,
  FINANCIAL_STATUS_BANKRUPT = 62,
  FINANCIAL_STATUS_DEFICIENT = 63,
  FINANCIAL_STATUS_DELINQUENT = 64,
  FINANCIAL_STATUS_BANKRUPT_AND_DEFICIENT = 65,
  FINANCIAL_STATUS_BANKRUPT_AND_DELINQUENT = 66,
  FINANCIAL_STATUS_DEFICIENT_AND_DELINQUENT = 67,
  FINANCIAL_STATUS_DEFICIENT_DELINQUENT_AND_BANKRUPT = 68,
  FINANCIAL_STATUS_LIQUIDATION = 69,
  FINANCIAL_STATUS_CREATIONS_SUSPENDED = 70,
  FINANCIAL_STATUS_REDEMPTIONS_SUSPENDED = 71,
};

enum class StockQuoteConditionCode : std::uint8_t {
  REGULAR_TWO_SIDED_OPEN = 1,
  REGULAR_ONE_SIDED_OPEN = 2,
  SLOW_ASK = 3,
  SLOW_BID = 4,
  SLOW_BID_AND_ASK = 5,
  SLOW_DUE_LRP_BID = 6,
  SLOW_DUE_LRP_ASK = 7,
  SLOW_DUE_SET_SLOW_LIST_BID_ASK = 9,
  MANUAL_ASK_AUTOMATED_BID = 10,
  MANUAL_BID_AUTOMATED_ASK = 11,
  MANUAL_BID_AND_ASK = 12,
  OPENING = 13,
  CLOSING = 14,
  CLOSED = 15,
  RESUME = 16,
  FAST_TRADING = 17,
  TRADING_RANGE_INDICATION = 18,
  MARKET_MAKER_QUOTES_CLOSED = 19,
  NON_FIRM = 20,
  NEWS_DISSEMINATION = 21,
  ORDER_INFLUX = 22,
  ORDER_IMBALANCE = 23,
  ADDITIONAL_INFORMATION = 26,
  NEWS_PENDING = 27,
  ADDITIONAL_INFORMATION_DUE_TO_RELATED_SECURITY = 28,
  DUE_TO_RELATED_SECURITY = 29,
  IN_VIEW_OF_COMMON = 30,
  NO_OPEN_NO_RESUME = 32,
  ON_DEMAND_AUCTION = 40,
  CASH_ONLY_SETTLEMENT = 41,
  NEXT_DAY_SETTLEMENT = 42,
  LULD_TRADING_PAUSE = 43,
  SLOW_DUE_LRP_BID_AND_ASK = 71,
  CORRECTED_PRICE_INDICATION = 81,
  SIP_GENERATED = 82,
  CROSSED_MARKET = 84,
  LOCKED_MARKET = 85,
  CQS_GENERATED = 94,
};

struct ConditionCodeMetadata {
  std::string_view enum_name;
  std::string_view display_name;
  bool updates_high_low = true;
  bool updates_open_close = true;
  bool updates_volume = true;
};

namespace detail {

enum class ConditionSetKind : std::uint8_t {
  raw_indices,
  stock_trade,
  stock_quote,
};

inline constexpr std::size_t condition_code_count = 96;

using ConditionCodeMetadataTable =
    std::array<std::optional<ConditionCodeMetadata>, condition_code_count>;

inline constexpr ConditionCodeMetadataTable
    stock_trade_condition_metadata = [] constexpr {
      ConditionCodeMetadataTable table{};
      table[1] = ConditionCodeMetadata{"ACQUISITION", "Acquisition", true, true, true};
      table[2] = ConditionCodeMetadata{"AVERAGE_PRICE_TRADE", "Average Price Trade", false, false, true};
      table[3] = ConditionCodeMetadata{"AUTOMATIC_EXECUTION", "Automatic Execution", true, true, true};
      table[4] = ConditionCodeMetadata{"BUNCHED_TRADE", "Bunched Trade", true, true, true};
      table[5] = ConditionCodeMetadata{"BUNCHED_SOLD_TRADE", "Bunched Sold Trade", true, false, true};
      table[6] = ConditionCodeMetadata{"CAP_ELECTION", "CAP Election", true, true, true};
      table[7] = ConditionCodeMetadata{"CASH_SALE", "Cash Sale", false, false, true};
      table[8] = ConditionCodeMetadata{"CLOSING_PRINTS", "Closing Prints", true, true, true};
      table[9] = ConditionCodeMetadata{"CROSS_TRADE", "Cross Trade", true, true, true};
      table[10] = ConditionCodeMetadata{"DERIVATIVELY_PRICED", "Derivatively Priced", true, false, true};
      table[11] = ConditionCodeMetadata{"DISTRIBUTION", "Distribution", true, true, true};
      table[12] = ConditionCodeMetadata{"FORM_T_EXTENDED_HOURS", "Form T/Extended Hours", false, false, true};
      table[13] = ConditionCodeMetadata{
          "EXTENDED_HOURS_SOLD_OUT_OF_SEQUENCE",
          "Extended Hours (Sold Out Of Sequence)",
          false,
          false,
          true};
      table[14] = ConditionCodeMetadata{"INTERMARKET_SWEEP", "Intermarket Sweep", true, true, true};
      table[15] = ConditionCodeMetadata{
          "MARKET_CENTER_OFFICIAL_CLOSE",
          "Market Center Official Close",
          false,
          false,
          false};
      table[16] = ConditionCodeMetadata{
          "MARKET_CENTER_OFFICIAL_OPEN",
          "Market Center Official Open",
          false,
          false,
          false};
      table[17] = ConditionCodeMetadata{"MARKET_CENTER_OPENING_TRADE", "Market Center Opening Trade", true, true, true};
      table[18] = ConditionCodeMetadata{
          "MARKET_CENTER_REOPENING_TRADE",
          "Market Center Reopening Trade",
          true,
          true,
          true};
      table[19] = ConditionCodeMetadata{"MARKET_CENTER_CLOSING_TRADE", "Market Center Closing Trade", true, true, true};
      table[20] = ConditionCodeMetadata{"NEXT_DAY", "Next Day", false, false, true};
      table[21] = ConditionCodeMetadata{"PRICE_VARIATION_TRADE", "Price Variation Trade", false, false, true};
      table[22] = ConditionCodeMetadata{"PRIOR_REFERENCE_PRICE", "Prior Reference Price", true, false, true};
      table[23] = ConditionCodeMetadata{"RULE_155_TRADE_AMEX", "Rule 155 Trade (AMEX)", true, true, true};
      table[24] = ConditionCodeMetadata{"RULE_127_NYSE_ONLY", "Rule 127 (NYSE Only)", true, true, true};
      table[25] = ConditionCodeMetadata{"OPENING_PRINTS", "Opening Prints", true, true, true};
      table[27] = ConditionCodeMetadata{
          "STOPPED_STOCK_REGULAR_TRADE",
          "Stopped Stock (Regular Trade)",
          true,
          true,
          true};
      table[28] = ConditionCodeMetadata{"RE_OPENING_PRINTS", "Re-Opening Prints", true, true, true};
      table[29] = ConditionCodeMetadata{"SELLER", "Seller", false, false, true};
      table[30] = ConditionCodeMetadata{"SOLD_LAST", "Sold Last", true, true, true};
      table[31] = ConditionCodeMetadata{"SOLD_LAST_AND_STOPPED_STOCK", "Sold Last and Stopped Stock", true, true, true};
      table[32] = ConditionCodeMetadata{"SOLD_OUT_OF_SEQUENCE", "Sold (Out Of Sequence)", true, false, true};
      table[33] = ConditionCodeMetadata{
          "SOLD_OUT_OF_SEQUENCE_AND_STOPPED_STOCK",
          "Sold (Out of Sequence) and Stopped Stock",
          true,
          false,
          true};
      table[34] = ConditionCodeMetadata{"SPLIT_TRADE", "Split Trade", true, true, true};
      table[35] = ConditionCodeMetadata{"STOCK_OPTION", "Stock Option", true, true, true};
      table[36] = ConditionCodeMetadata{"YELLOW_FLAG_REGULAR_TRADE", "Yellow Flag Regular Trade", true, true, true};
      table[37] = ConditionCodeMetadata{"ODD_LOT_TRADE", "Odd Lot Trade", false, false, true};
      table[38] = ConditionCodeMetadata{
          "CORRECTED_CONSOLIDATED_CLOSE_PER_LISTING_MARKET",
          "Corrected Consolidated Close (per listing market)",
          true,
          true,
          false};
      table[41] = ConditionCodeMetadata{"TRADE_THRU_EXEMPT", "Trade Thru Exempt", true, true, true};
      table[52] = ConditionCodeMetadata{"CONTINGENT_TRADE", "Contingent Trade", false, false, true};
      table[53] = ConditionCodeMetadata{"QUALIFIED_CONTINGENT_TRADE", "Qualified Contingent Trade", false, false, true};
      table[55] = ConditionCodeMetadata{
          "OPENING_REOPENING_TRADE_DETAIL",
          "Opening Reopening Trade Detail",
          true,
          true,
          true};
      table[57] = ConditionCodeMetadata{
          "SHORT_SALE_RESTRICTION_ACTIVATED",
          "Short Sale Restriction Activated",
          true,
          true,
          true};
      table[58] = ConditionCodeMetadata{
          "SHORT_SALE_RESTRICTION_CONTINUED",
          "Short Sale Restriction Continued",
          true,
          true,
          true};
      table[59] = ConditionCodeMetadata{
          "SHORT_SALE_RESTRICTION_DEACTIVATED",
          "Short Sale Restriction Deactivated",
          true,
          true,
          true};
      table[60] = ConditionCodeMetadata{
          "SHORT_SALE_RESTRICTION_IN_EFFECT",
          "Short Sale Restriction In Effect",
          true,
          true,
          true};
      table[62] = ConditionCodeMetadata{"FINANCIAL_STATUS_BANKRUPT", "Financial Status - Bankrupt", true, true, true};
      table[63] = ConditionCodeMetadata{"FINANCIAL_STATUS_DEFICIENT", "Financial Status - Deficient", true, true, true};
      table[64] = ConditionCodeMetadata{"FINANCIAL_STATUS_DELINQUENT", "Financial Status - Delinquent", true, true, true};
      table[65] = ConditionCodeMetadata{
          "FINANCIAL_STATUS_BANKRUPT_AND_DEFICIENT",
          "Financial Status - Bankrupt and Deficient",
          true,
          true,
          true};
      table[66] = ConditionCodeMetadata{
          "FINANCIAL_STATUS_BANKRUPT_AND_DELINQUENT",
          "Financial Status - Bankrupt and Delinquent",
          true,
          true,
          true};
      table[67] = ConditionCodeMetadata{
          "FINANCIAL_STATUS_DEFICIENT_AND_DELINQUENT",
          "Financial Status - Deficient and Delinquent",
          true,
          true,
          true};
      table[68] = ConditionCodeMetadata{
          "FINANCIAL_STATUS_DEFICIENT_DELINQUENT_AND_BANKRUPT",
          "Financial Status - Deficient, Delinquent, and Bankrupt",
          true,
          true,
          true};
      table[69] = ConditionCodeMetadata{"FINANCIAL_STATUS_LIQUIDATION", "Financial Status - Liquidation", true, true, true};
      table[70] = ConditionCodeMetadata{
          "FINANCIAL_STATUS_CREATIONS_SUSPENDED",
          "Financial Status - Creations Suspended",
          true,
          true,
          true};
      table[71] = ConditionCodeMetadata{
          "FINANCIAL_STATUS_REDEMPTIONS_SUSPENDED",
          "Financial Status - Redemptions Suspended",
          true,
          true,
          true};
      return table;
    }();

inline constexpr ConditionCodeMetadataTable
    stock_quote_condition_metadata = [] constexpr {
      ConditionCodeMetadataTable table{};
      table[1] = ConditionCodeMetadata{"REGULAR_TWO_SIDED_OPEN", "Regular Two-Sided Open", true, true, true};
      table[2] = ConditionCodeMetadata{"REGULAR_ONE_SIDED_OPEN", "Regular One-Sided Open", true, true, true};
      table[3] = ConditionCodeMetadata{"SLOW_ASK", "Slow Ask", true, true, true};
      table[4] = ConditionCodeMetadata{"SLOW_BID", "Slow Bid", true, true, true};
      table[5] = ConditionCodeMetadata{"SLOW_BID_AND_ASK", "Slow Bid and Ask", true, true, true};
      table[6] = ConditionCodeMetadata{"SLOW_DUE_LRP_BID", "Slow Due LRP Bid", true, true, true};
      table[7] = ConditionCodeMetadata{"SLOW_DUE_LRP_ASK", "Slow Due LRP Ask", true, true, true};
      table[9] = ConditionCodeMetadata{
          "SLOW_DUE_SET_SLOW_LIST_BID_ASK",
          "Slow Due Set Slow List Bid Ask",
          true,
          true,
          true};
      table[10] = ConditionCodeMetadata{"MANUAL_ASK_AUTOMATED_BID", "Manual Ask Automated Bid", true, true, true};
      table[11] = ConditionCodeMetadata{"MANUAL_BID_AUTOMATED_ASK", "Manual Bid Automated Ask", true, true, true};
      table[12] = ConditionCodeMetadata{"MANUAL_BID_AND_ASK", "Manual Bid and Ask", true, true, true};
      table[13] = ConditionCodeMetadata{"OPENING", "Opening", true, true, true};
      table[14] = ConditionCodeMetadata{"CLOSING", "Closing", true, true, true};
      table[15] = ConditionCodeMetadata{"CLOSED", "Closed", true, true, true};
      table[16] = ConditionCodeMetadata{"RESUME", "Resume", true, true, true};
      table[17] = ConditionCodeMetadata{"FAST_TRADING", "Fast Trading", true, true, true};
      table[18] = ConditionCodeMetadata{"TRADING_RANGE_INDICATION", "Trading Range Indication", true, true, true};
      table[19] = ConditionCodeMetadata{"MARKET_MAKER_QUOTES_CLOSED", "Market Maker Quotes Closed", true, true, true};
      table[20] = ConditionCodeMetadata{"NON_FIRM", "Non-Firm", true, true, true};
      table[21] = ConditionCodeMetadata{"NEWS_DISSEMINATION", "News Dissemination", true, true, true};
      table[22] = ConditionCodeMetadata{"ORDER_INFLUX", "Order Influx", true, true, true};
      table[23] = ConditionCodeMetadata{"ORDER_IMBALANCE", "Order Imbalance", true, true, true};
      table[26] = ConditionCodeMetadata{"ADDITIONAL_INFORMATION", "Additional Information", true, true, true};
      table[27] = ConditionCodeMetadata{"NEWS_PENDING", "News Pending", true, true, true};
      table[28] = ConditionCodeMetadata{
          "ADDITIONAL_INFORMATION_DUE_TO_RELATED_SECURITY",
          "Additional Information Due To Related Security",
          true,
          true,
          true};
      table[29] = ConditionCodeMetadata{"DUE_TO_RELATED_SECURITY", "Due To Related Security", true, true, true};
      table[30] = ConditionCodeMetadata{"IN_VIEW_OF_COMMON", "In View Of Common", true, true, true};
      table[32] = ConditionCodeMetadata{"NO_OPEN_NO_RESUME", "No Open No Resume", true, true, true};
      table[40] = ConditionCodeMetadata{"ON_DEMAND_AUCTION", "On Demand Auction", true, true, true};
      table[41] = ConditionCodeMetadata{"CASH_ONLY_SETTLEMENT", "Cash Only Settlement", true, true, true};
      table[42] = ConditionCodeMetadata{"NEXT_DAY_SETTLEMENT", "Next Day Settlement", true, true, true};
      table[43] = ConditionCodeMetadata{"LULD_TRADING_PAUSE", "LULD Trading Pause", true, true, true};
      table[71] = ConditionCodeMetadata{"SLOW_DUE_LRP_BID_AND_ASK", "Slow Due LRP Bid and Ask", true, true, true};
      table[81] = ConditionCodeMetadata{"CORRECTED_PRICE_INDICATION", "Corrected Price Indication", true, true, true};
      table[82] = ConditionCodeMetadata{"SIP_GENERATED", "SIP Generated", true, true, true};
      table[84] = ConditionCodeMetadata{"CROSSED_MARKET", "Crossed Market", true, true, true};
      table[85] = ConditionCodeMetadata{"LOCKED_MARKET", "Locked Market", true, true, true};
      table[94] = ConditionCodeMetadata{"CQS_GENERATED", "CQS Generated", true, true, true};
      return table;
    }();

inline constexpr const std::optional<ConditionCodeMetadata>& condition_metadata_at(
    ConditionSetKind kind,
    std::size_t index) {
  if (index >= condition_code_count) {
    static_assert(condition_code_count == 96);
    return stock_trade_condition_metadata[0];
  }

  switch (kind) {
    case ConditionSetKind::stock_trade:
      return stock_trade_condition_metadata[index];
    case ConditionSetKind::stock_quote:
      return stock_quote_condition_metadata[index];
    case ConditionSetKind::raw_indices:
      return stock_trade_condition_metadata[0];
  }

  return stock_trade_condition_metadata[0];
}

struct ConditionCodeMask {
  static constexpr std::size_t word_count = (condition_code_count + 63) / 64;
  std::array<std::uint64_t, word_count> words{};

  constexpr void set(std::size_t index) {
    words[index / 64] |= std::uint64_t{1} << (index % 64);
  }

  bool intersects_packed(const void* packed_data) const {
    const auto* bytes = static_cast<const std::uint8_t*>(packed_data);
    const std::uint64_t low =
        static_cast<std::uint64_t>(bytes[0]) |
        (static_cast<std::uint64_t>(bytes[1]) << 8U) |
        (static_cast<std::uint64_t>(bytes[2]) << 16U) |
        (static_cast<std::uint64_t>(bytes[3]) << 24U) |
        (static_cast<std::uint64_t>(bytes[4]) << 32U) |
        (static_cast<std::uint64_t>(bytes[5]) << 40U) |
        (static_cast<std::uint64_t>(bytes[6]) << 48U) |
        (static_cast<std::uint64_t>(bytes[7]) << 56U);
    const std::uint64_t high =
        static_cast<std::uint64_t>(bytes[8]) |
        (static_cast<std::uint64_t>(bytes[9]) << 8U) |
        (static_cast<std::uint64_t>(bytes[10]) << 16U) |
        (static_cast<std::uint64_t>(bytes[11]) << 24U);
    return ((low & words[0]) != 0) || ((high & words[1]) != 0);
  }
};

template <bool ConditionCodeMetadata::* Rule>
consteval ConditionCodeMask make_rule_exclusion_mask(
    const ConditionCodeMetadataTable& table) {
  ConditionCodeMask mask{};
  for (std::size_t index = 0; index < table.size(); ++index) {
    if (table[index].has_value() && !((*table[index]).*Rule)) {
      mask.set(index);
    }
  }
  return mask;
}

inline constexpr ConditionCodeMask stock_trade_high_low_exclusion_mask =
    make_rule_exclusion_mask<&ConditionCodeMetadata::updates_high_low>(
        stock_trade_condition_metadata);
inline constexpr ConditionCodeMask stock_trade_open_close_exclusion_mask =
    make_rule_exclusion_mask<&ConditionCodeMetadata::updates_open_close>(
        stock_trade_condition_metadata);
inline constexpr ConditionCodeMask stock_trade_volume_exclusion_mask =
    make_rule_exclusion_mask<&ConditionCodeMetadata::updates_volume>(
        stock_trade_condition_metadata);
inline constexpr ConditionCodeMask stock_quote_high_low_exclusion_mask =
    make_rule_exclusion_mask<&ConditionCodeMetadata::updates_high_low>(
        stock_quote_condition_metadata);
inline constexpr ConditionCodeMask stock_quote_open_close_exclusion_mask =
    make_rule_exclusion_mask<&ConditionCodeMetadata::updates_open_close>(
        stock_quote_condition_metadata);
inline constexpr ConditionCodeMask stock_quote_volume_exclusion_mask =
    make_rule_exclusion_mask<&ConditionCodeMetadata::updates_volume>(
        stock_quote_condition_metadata);

inline std::bitset<condition_code_count> bitset_from_mask(
    const ConditionCodeMask& mask) {
  std::bitset<condition_code_count> bits;
  for (std::size_t word_index = 0; word_index < mask.words.size(); ++word_index) {
    std::uint64_t word = mask.words[word_index];
    std::size_t bit_offset = word_index * 64;
    while (word != 0) {
      const auto bit_index =
          static_cast<std::size_t>(std::countr_zero(word));
      bits.set(bit_offset + bit_index);
      word &= word - 1;
    }
  }
  return bits;
}

inline const std::bitset<condition_code_count> stock_trade_high_low_exclusion_bits =
    bitset_from_mask(stock_trade_high_low_exclusion_mask);
inline const std::bitset<condition_code_count> stock_trade_open_close_exclusion_bits =
    bitset_from_mask(stock_trade_open_close_exclusion_mask);
inline const std::bitset<condition_code_count> stock_trade_volume_exclusion_bits =
    bitset_from_mask(stock_trade_volume_exclusion_mask);
inline const std::bitset<condition_code_count> stock_quote_high_low_exclusion_bits =
    bitset_from_mask(stock_quote_high_low_exclusion_mask);
inline const std::bitset<condition_code_count> stock_quote_open_close_exclusion_bits =
    bitset_from_mask(stock_quote_open_close_exclusion_mask);
inline const std::bitset<condition_code_count> stock_quote_volume_exclusion_bits =
    bitset_from_mask(stock_quote_volume_exclusion_mask);

inline bool condition_bits_clear(
    const std::bitset<condition_code_count>& conditions,
    const std::bitset<condition_code_count>& exclusion_bits) {
  return (conditions & exclusion_bits).none();
}

inline bool stock_trade_updates_high_low(const std::bitset<96>& conditions) {
  return condition_bits_clear(conditions, stock_trade_high_low_exclusion_bits);
}

inline bool stock_trade_updates_open_close(const std::bitset<96>& conditions) {
  return condition_bits_clear(conditions, stock_trade_open_close_exclusion_bits);
}

inline bool stock_trade_updates_volume(const std::bitset<96>& conditions) {
  return condition_bits_clear(conditions, stock_trade_volume_exclusion_bits);
}

inline bool stock_quote_updates_high_low(const std::bitset<96>& conditions) {
  return condition_bits_clear(conditions, stock_quote_high_low_exclusion_bits);
}

inline bool stock_quote_updates_open_close(const std::bitset<96>& conditions) {
  return condition_bits_clear(conditions, stock_quote_open_close_exclusion_bits);
}

inline bool stock_quote_updates_volume(const std::bitset<96>& conditions) {
  return condition_bits_clear(conditions, stock_quote_volume_exclusion_bits);
}

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

inline double parse_nullable_double(std::string_view text, std::string_view field_name) {
  if (text.empty()) {
    static_cast<void>(field_name);
    return std::numeric_limits<double>::quiet_NaN();
  }
  return parse_double(text, field_name);
}

struct DecimalQuantity {
  std::uint64_t coefficient = 0;
  std::uint8_t scale = 0;

  bool operator==(const DecimalQuantity&) const = default;
};

inline DecimalQuantity parse_decimal_quantity(
    std::string_view text,
    std::string_view field_name) {
  if (text.empty()) {
    return {};
  }

  std::size_t position = 0;
  if (text.front() == '+') {
    position = 1;
  } else if (text.front() == '-') {
    std::ostringstream message;
    message << "decimal quantity must be non-negative " << field_name << ": " << text;
    throw std::invalid_argument(message.str());
  }

  DecimalQuantity result;
  bool saw_digit = false;
  bool saw_decimal_point = false;
  std::size_t scale = 0;
  constexpr std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();

  for (; position < text.size(); ++position) {
    const char character = text[position];
    if (character == '.') {
      if (saw_decimal_point) {
        std::ostringstream message;
        message << "unable to parse decimal quantity field " << field_name
                << ": " << text;
        throw std::invalid_argument(message.str());
      }
      saw_decimal_point = true;
      continue;
    }
    if (character < '0' || character > '9') {
      std::ostringstream message;
      message << "unable to parse decimal quantity field " << field_name
              << ": " << text;
      throw std::invalid_argument(message.str());
    }

    saw_digit = true;
    const auto digit = static_cast<std::uint64_t>(character - '0');
    if (result.coefficient > (maximum - digit) / 10U) {
      std::ostringstream message;
      message << "decimal quantity field out of range " << field_name << ": " << text;
      throw std::out_of_range(message.str());
    }
    result.coefficient = result.coefficient * 10U + digit;
    if (saw_decimal_point) {
      ++scale;
      if (scale > std::numeric_limits<std::uint8_t>::max()) {
        std::ostringstream message;
        message << "decimal quantity scale out of range " << field_name << ": " << text;
        throw std::out_of_range(message.str());
      }
    }
  }

  if (!saw_digit) {
    std::ostringstream message;
    message << "unable to parse decimal quantity field " << field_name << ": " << text;
    throw std::invalid_argument(message.str());
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

inline double decimal_quantity_to_double(const DecimalQuantity& quantity) {
  return static_cast<double>(quantity.coefficient) *
      std::pow(10.0, -static_cast<int>(quantity.scale));
}

inline std::string decimal_quantity_string(const DecimalQuantity& quantity) {
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

inline bool is_date_filename_prefix(std::string_view text) {
  return text.size() >= 10 &&
         std::isdigit(static_cast<unsigned char>(text[0])) &&
         std::isdigit(static_cast<unsigned char>(text[1])) &&
         std::isdigit(static_cast<unsigned char>(text[2])) &&
         std::isdigit(static_cast<unsigned char>(text[3])) &&
         text[4] == '-' &&
         std::isdigit(static_cast<unsigned char>(text[5])) &&
         std::isdigit(static_cast<unsigned char>(text[6])) &&
         text[7] == '-' &&
         std::isdigit(static_cast<unsigned char>(text[8])) &&
         std::isdigit(static_cast<unsigned char>(text[9]));
}

inline std::string date_directory_name_from_filename(
    const std::filesystem::path& path) {
  const std::string filename = path.filename().string();
  if (!is_date_filename_prefix(filename)) {
    std::ostringstream message;
    message << "database input filename must begin with YYYY-MM-DD: " << path;
    throw std::invalid_argument(message.str());
  }
  const auto digit = [&filename](std::size_t index) -> unsigned {
    return static_cast<unsigned>(filename[index] - '0');
  };
  const int year =
      static_cast<int>(digit(0) * 1000 + digit(1) * 100 + digit(2) * 10 + digit(3));
  const unsigned month = digit(5) * 10 + digit(6);
  const unsigned day = digit(8) * 10 + digit(9);
  const std::chrono::year_month_day ymd{
      std::chrono::year{year},
      std::chrono::month{month},
      std::chrono::day{day}};
  if (!ymd.ok()) {
    std::ostringstream message;
    message << "database input filename must begin with a valid YYYY-MM-DD date: "
            << path;
    throw std::invalid_argument(message.str());
  }
  return filename.substr(0, 10);
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

class AtomicBinaryRecordWriter {
 public:
  explicit AtomicBinaryRecordWriter(std::size_t buffer_size = 1U << 20)
      : writer_(buffer_size) {}

  AtomicBinaryRecordWriter(const AtomicBinaryRecordWriter&) = delete;
  AtomicBinaryRecordWriter& operator=(const AtomicBinaryRecordWriter&) = delete;

  void open(const std::filesystem::path& final_path) {
    if (active_) {
      throw std::logic_error(
          "atomic database output must be committed before opening another file");
    }
    final_path_ = final_path;
    incomplete_path_ = final_path;
    incomplete_path_ += ".incomplete";
    writer_.open(incomplete_path_);
    active_ = true;
  }

  template <std::size_t PackedSize>
  void write(const PackedBuffer<PackedSize>& data) {
    writer_.write(data);
  }

  void commit() {
    if (!active_) {
      return;
    }
    writer_.close();
    active_ = false;

    std::error_code error;
    std::filesystem::rename(incomplete_path_, final_path_, error);
    if (error) {
      std::ostringstream message;
      message << "unable to publish database output file " << final_path_
              << " from " << incomplete_path_ << ": " << error.message();
      throw std::runtime_error(message.str());
    }
  }

 private:
  BinaryRecordWriter writer_;
  std::filesystem::path final_path_;
  std::filesystem::path incomplete_path_;
  bool active_ = false;
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

inline float read_float32_le_at(const void* data, std::size_t offset) {
  static_assert(sizeof(float) == sizeof(std::uint32_t));
  static_assert(std::numeric_limits<float>::is_iec559);
  return std::bit_cast<float>(read_uint32_le_at(data, offset));
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

template <std::size_t PackedSize>
void write_float32_le(
    PackedBuffer<PackedSize>& output,
    std::size_t& offset,
    float value) {
  static_assert(sizeof(float) == sizeof(std::uint32_t));
  static_assert(std::numeric_limits<float>::is_iec559);
  write_unsigned_le(output, offset, std::bit_cast<std::uint32_t>(value));
}

inline float read_float32_le(std::string_view input, std::size_t& offset) {
  static_assert(sizeof(float) == sizeof(std::uint32_t));
  static_assert(std::numeric_limits<float>::is_iec559);
  return std::bit_cast<float>(read_unsigned_le<std::uint32_t>(input, offset));
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

inline std::array<PyObject*, condition_code_count>& condition_enum_members(
    ConditionSetKind kind) {
  static auto* stock_trade_members = new std::array<PyObject*, condition_code_count>{};
  static auto* stock_quote_members = new std::array<PyObject*, condition_code_count>{};

  switch (kind) {
    case ConditionSetKind::stock_trade:
      return *stock_trade_members;
    case ConditionSetKind::stock_quote:
      return *stock_quote_members;
    case ConditionSetKind::raw_indices:
      return *stock_trade_members;
  }

  return *stock_trade_members;
}

inline const ConditionCodeMetadataTable& condition_metadata_table(
    ConditionSetKind kind) {
  switch (kind) {
    case ConditionSetKind::stock_trade:
      return stock_trade_condition_metadata;
    case ConditionSetKind::stock_quote:
      return stock_quote_condition_metadata;
    case ConditionSetKind::raw_indices:
      return stock_trade_condition_metadata;
  }

  return stock_trade_condition_metadata;
}

inline void install_condition_enum_members(
    ConditionSetKind kind,
    nanobind::handle enum_type) {
  auto& members = condition_enum_members(kind);
  const auto& metadata = condition_metadata_table(kind);

  for (std::size_t index = 0; index < members.size(); ++index) {
    Py_CLEAR(members[index]);
    if (!metadata[index].has_value()) {
      continue;
    }

    nanobind::object member = enum_type.attr(metadata[index]->enum_name.data());
    Py_INCREF(member.ptr());
    members[index] = member.ptr();
  }
}

inline PyObject* condition_value_new_ref(
    ConditionSetKind kind,
    std::size_t index) {
  if (kind != ConditionSetKind::raw_indices && index < condition_code_count) {
    PyObject* member = condition_enum_members(kind)[index];
    if (member != nullptr) {
      Py_INCREF(member);
      return member;
    }
  }

  return PyLong_FromSize_t(index);
}

inline nanobind::object bit_indices_frozenset(
    const std::bitset<96>& bits,
    ConditionSetKind kind) {
  struct BitsetKeyHash {
    std::size_t operator()(const std::pair<ConditionSetKind, std::bitset<96>>& value)
        const noexcept {
      std::size_t seed = std::hash<unsigned>{}(
          static_cast<unsigned>(value.first));
      seed ^= bitset_hash(value.second) + 0x9e3779b97f4a7c15ULL + (seed << 6U) +
          (seed >> 2U);
      return seed;
    }
  };

  using InternTable = std::unordered_map<
      std::pair<ConditionSetKind, std::bitset<96>>,
      PyObject*,
      BitsetKeyHash>;
  static InternTable* interned_sets = new InternTable();
  static std::mutex interned_sets_mutex;

  const std::pair<ConditionSetKind, std::bitset<96>> key{kind, bits};
  std::lock_guard<std::mutex> lock(interned_sets_mutex);
  if (const auto found = interned_sets->find(key); found != interned_sets->end()) {
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

    nanobind::object value =
        nanobind::steal<nanobind::object>(condition_value_new_ref(kind, index));
    if (!value.is_valid() || PySet_Add(result.ptr(), value.ptr()) != 0) {
      throw nanobind::python_error();
    }
  }

  Py_INCREF(result.ptr());
  interned_sets->emplace(key, result.ptr());
  return result;
}

inline nanobind::object bit_indices_frozenset(const std::bitset<96>& bits) {
  return bit_indices_frozenset(bits, ConditionSetKind::raw_indices);
}

static constexpr std::uint16_t option_trade_condition_min = 201;
static constexpr std::uint16_t option_trade_condition_max = 248;
static constexpr std::size_t option_trade_condition_count =
    option_trade_condition_max - option_trade_condition_min + 1;
static constexpr std::size_t option_trade_condition_bytes =
    (option_trade_condition_count + 7) / 8;

struct OptionConditionBits {
  std::array<std::uint8_t, option_trade_condition_bytes> bytes{};

  void set(std::size_t offset) {
    bytes[offset / 8] |= static_cast<std::uint8_t>(1U << (offset % 8));
  }

  bool test(std::size_t offset) const {
    return (bytes[offset / 8] & static_cast<std::uint8_t>(1U << (offset % 8))) != 0;
  }

  bool operator==(const OptionConditionBits& other) const = default;
};

inline std::uint8_t parse_option_condition_offset(
    std::string_view token,
    std::string_view field_name) {
  const auto code = parse_integer<std::uint16_t>(token, field_name);
  if (code < option_trade_condition_min || code > option_trade_condition_max) {
    std::ostringstream message;
    message << "option trade condition code out of range " << field_name << ": "
            << code;
    throw std::out_of_range(message.str());
  }
  return static_cast<std::uint8_t>(code - option_trade_condition_min);
}

inline OptionConditionBits parse_option_condition_bits(
    std::string_view text,
    std::string_view field_name) {
  OptionConditionBits result;
  if (text.empty()) {
    return result;
  }

  std::size_t position = 0;
  while (position < text.size()) {
    const std::size_t start = position;
    const auto comma = text.find(',', start);
    const std::string_view token = comma == std::string_view::npos
        ? text.substr(start)
        : text.substr(start, comma - start);
    if (token.empty()) {
      throw std::invalid_argument("empty option trade condition code");
    }

    const std::uint8_t offset = parse_option_condition_offset(token, field_name);
    result.set(offset);

    if (comma == std::string_view::npos) {
      break;
    }
    position = comma + 1;
  }

  return result;
}

inline nanobind::object option_conditions_frozenset(
    const OptionConditionBits& conditions) {
  nanobind::object result = nanobind::steal<nanobind::object>(PyFrozenSet_New(nullptr));
  if (!result.is_valid()) {
    throw nanobind::python_error();
  }

  for (std::size_t index = 0; index < option_trade_condition_count; ++index) {
    if (!conditions.test(index)) {
      continue;
    }
    const auto code = static_cast<unsigned>(
        option_trade_condition_min + index);
    nanobind::object value = nanobind::steal<nanobind::object>(
        PyLong_FromUnsignedLong(code));
    if (!value.is_valid() || PySet_Add(result.ptr(), value.ptr()) != 0) {
      throw nanobind::python_error();
    }
  }

  return result;
}

inline std::string option_conditions_repr(const OptionConditionBits& conditions) {
  std::ostringstream out;
  bool first = true;
  for (std::size_t index = 0; index < option_trade_condition_count; ++index) {
    if (!conditions.test(index)) {
      continue;
    }
    if (first) {
      out << "frozenset({";
      first = false;
    } else {
      out << ", ";
    }
    out << static_cast<unsigned>(
        option_trade_condition_min + index);
  }

  if (first) {
    return "frozenset()";
  }
  out << "})";
  return out.str();
}

inline std::size_t option_condition_hash(const OptionConditionBits& conditions) {
  std::size_t seed = 0;
  for (const std::uint8_t value : conditions.bytes) {
    seed ^= std::hash<unsigned>{}(value) + 0x9e3779b97f4a7c15ULL +
        (seed << 6U) + (seed >> 2U);
  }
  return seed;
}

struct OptionSymbolParts {
  std::string root;
  std::string expiration;
  char right = '\0';
  std::uint32_t strike_millis = 0;
  double strike = 0.0;
};

inline std::uint32_t parse_fixed_unsigned_digits(
    std::string_view text,
    std::string_view field_name) {
  if (text.empty()) {
    std::ostringstream message;
    message << "empty " << field_name;
    throw std::invalid_argument(message.str());
  }

  std::uint32_t value = 0;
  for (const char ch : text) {
    if (!std::isdigit(static_cast<unsigned char>(ch))) {
      std::ostringstream message;
      message << "invalid decimal digit in " << field_name << ": " << text;
      throw std::invalid_argument(message.str());
    }
    value = value * 10U + static_cast<std::uint32_t>(ch - '0');
  }
  return value;
}

inline std::string option_expiration_string(
    std::uint32_t year,
    std::uint32_t month,
    std::uint32_t day) {
  const std::chrono::year_month_day expiration{
      std::chrono::year(static_cast<int>(year)) /
      std::chrono::month(month) /
      std::chrono::day(day)};
  if (!expiration.ok()) {
    std::ostringstream message;
    message << "invalid option expiration date: " << year << '-'
            << month << '-' << day;
    throw std::invalid_argument(message.str());
  }

  std::string result(10, '0');
  result[0] = static_cast<char>('0' + (year / 1000U) % 10U);
  result[1] = static_cast<char>('0' + (year / 100U) % 10U);
  result[2] = static_cast<char>('0' + (year / 10U) % 10U);
  result[3] = static_cast<char>('0' + year % 10U);
  result[4] = '-';
  result[5] = static_cast<char>('0' + (month / 10U) % 10U);
  result[6] = static_cast<char>('0' + month % 10U);
  result[7] = '-';
  result[8] = static_cast<char>('0' + (day / 10U) % 10U);
  result[9] = static_cast<char>('0' + day % 10U);
  return result;
}

inline OptionSymbolParts parse_option_symbol(std::string_view ticker) {
  if (!ticker.starts_with("O:")) {
    std::ostringstream message;
    message << "option ticker must start with O: " << ticker;
    throw std::invalid_argument(message.str());
  }

  const std::string_view body = ticker.substr(2);
  static constexpr std::size_t suffix_size = 6 + 1 + 8;
  if (body.size() <= suffix_size) {
    std::ostringstream message;
    message << "option ticker is too short: " << ticker;
    throw std::invalid_argument(message.str());
  }

  const std::size_t suffix_start = body.size() - suffix_size;
  const std::string_view root = body.substr(0, suffix_start);
  const std::string_view expiration = body.substr(suffix_start, 6);
  const char right = body[suffix_start + 6];
  const std::string_view strike = body.substr(suffix_start + 7, 8);

  if (right != 'C' && right != 'P') {
    std::ostringstream message;
    message << "option ticker right must be C or P: " << ticker;
    throw std::invalid_argument(message.str());
  }

  const std::uint32_t year =
      2000U + parse_fixed_unsigned_digits(expiration.substr(0, 2), "expiration year");
  const std::uint32_t month =
      parse_fixed_unsigned_digits(expiration.substr(2, 2), "expiration month");
  const std::uint32_t day =
      parse_fixed_unsigned_digits(expiration.substr(4, 2), "expiration day");
  const std::uint32_t strike_millis =
      parse_fixed_unsigned_digits(strike, "strike");

  OptionSymbolParts result;
  result.root.assign(root);
  result.expiration = option_expiration_string(year, month, day);
  result.right = right;
  result.strike_millis = strike_millis;
  result.strike = static_cast<double>(strike_millis) / 1000.0;
  return result;
}

inline std::string option_strike_component(std::uint32_t strike_millis) {
  if (strike_millis > 99'999'999U) {
    throw std::out_of_range("option strike component exceeds 8 digits");
  }
  std::string result(8, '0');
  for (std::size_t index = 0; index < result.size(); ++index) {
    const std::size_t divisor_index = result.size() - index - 1;
    std::uint32_t divisor = 1;
    for (std::size_t digit = 0; digit < divisor_index; ++digit) {
      divisor *= 10U;
    }
    result[index] =
        static_cast<char>('0' + ((strike_millis / divisor) % 10U));
  }
  return result;
}

inline std::uint32_t strike_millis_from_double(double strike) {
  if (!std::isfinite(strike) || strike < 0.0) {
    throw std::invalid_argument("option strike must be a finite non-negative number");
  }
  const double scaled = std::round(strike * 1000.0);
  if (scaled < 0.0 || scaled > 99'999'999.0) {
    throw std::out_of_range("option strike is out of range");
  }
  return static_cast<std::uint32_t>(scaled);
}

inline char option_right_from_string(std::string_view right) {
  if (right.size() != 1 || (right[0] != 'C' && right[0] != 'P')) {
    throw std::invalid_argument("option right must be C or P");
  }
  return right[0];
}

inline std::string option_contract_key(
    std::string_view root,
    std::string_view expiration,
    char right,
    std::uint32_t strike_millis) {
  if (root.empty()) {
    throw std::invalid_argument("option root must not be empty");
  }
  if (root.find('/') != std::string_view::npos) {
    throw std::invalid_argument("option root must not contain '/'");
  }
  if (expiration.find('/') != std::string_view::npos) {
    throw std::invalid_argument("option expiration must not contain '/'");
  }
  if (right != 'C' && right != 'P') {
    throw std::invalid_argument("option right must be C or P");
  }

  std::string key;
  key.reserve(root.size() + expiration.size() + 1 + 8 + 3);
  key.append(root);
  key.push_back('/');
  key.append(expiration);
  key.push_back('/');
  key.push_back(right);
  key.push_back('/');
  key.append(option_strike_component(strike_millis));
  return key;
}

inline std::string option_contract_key(
    std::string_view root,
    std::string_view expiration,
    std::string_view right,
    double strike) {
  return option_contract_key(
      root,
      expiration,
      option_right_from_string(right),
      strike_millis_from_double(strike));
}

inline OptionSymbolParts parse_option_contract_key(std::string_view key) {
  const std::size_t first = key.find('/');
  const std::size_t second =
      first == std::string_view::npos ? std::string_view::npos : key.find('/', first + 1);
  const std::size_t third =
      second == std::string_view::npos ? std::string_view::npos : key.find('/', second + 1);
  if (first == std::string_view::npos ||
      second == std::string_view::npos ||
      third == std::string_view::npos ||
      key.find('/', third + 1) != std::string_view::npos) {
    std::ostringstream message;
    message << "invalid option contract database key: " << key;
    throw std::invalid_argument(message.str());
  }

  const std::string_view root = key.substr(0, first);
  const std::string_view expiration = key.substr(first + 1, second - first - 1);
  const std::string_view right_text = key.substr(second + 1, third - second - 1);
  const std::string_view strike_text = key.substr(third + 1);
  if (strike_text.size() != 8) {
    std::ostringstream message;
    message << "option contract strike component must be 8 digits: " << key;
    throw std::invalid_argument(message.str());
  }

  OptionSymbolParts result;
  result.root.assign(root);
  result.expiration.assign(expiration);
  result.right = option_right_from_string(right_text);
  result.strike_millis = parse_fixed_unsigned_digits(strike_text, "strike");
  result.strike = static_cast<double>(result.strike_millis) / 1000.0;
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
  static std::mutex interned_tickers_mutex;

  std::lock_guard<std::mutex> lock(interned_tickers_mutex);
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

inline std::string bit_indices_repr(
    const std::bitset<96>& bits,
    ConditionSetKind kind = ConditionSetKind::raw_indices) {
  std::ostringstream out;
  bool first = true;
  const auto& metadata = condition_metadata_table(kind);
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
    if (kind != ConditionSetKind::raw_indices &&
        index < metadata.size() &&
        metadata[index].has_value()) {
      out << metadata[index]->enum_name;
    } else {
      out << index;
    }
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
    std::lock_guard<std::mutex> lock(mutex_);
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
  std::mutex mutex_;
};

template <std::size_t Count, typename Factory>
nanobind::object cached_python_object(
    std::unique_ptr<LazyPythonObjectCache<Count>>& cache,
    std::size_t index,
    Factory&& factory) {
  static std::mutex cache_pointer_mutex;
  std::lock_guard<std::mutex> lock(cache_pointer_mutex);
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
    std::lock_guard<std::mutex> lock(mutex_);
    for (PyObject*& object : objects_) {
      Py_XDECREF(object);
      object = nullptr;
    }
  }

  template <typename Factory>
  nanobind::object get(std::size_t index, Factory&& factory) const {
    std::lock_guard<std::mutex> lock(mutex_);
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
  mutable std::mutex mutex_;
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
  static constexpr std::size_t packed_size = 78;
  static constexpr std::size_t packed_participant_timestamp_offset = 25;
  static constexpr std::size_t packed_price_offset = 33;
  static constexpr std::size_t packed_sip_timestamp_offset = 49;
  static constexpr std::size_t packed_size_offset = 57;
  static constexpr std::size_t packed_size_scale_offset = 65;
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
    decimal_size_attribute,
    size_coefficient_attribute,
    size_scale_attribute,
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
  detail::DecimalQuantity exact_size;
  std::int32_t correction = 0;
  double size = 0.0;
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
        exact_size(other.exact_size),
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
    exact_size = other.exact_size;
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
    result.exact_size = detail::parse_decimal_quantity(fields[9], "size");
    result.size = detail::decimal_quantity_to_double(result.exact_size);
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
    result.exact_size.coefficient =
        detail::read_unsigned_le<std::uint64_t>(packed_data, offset);
    result.exact_size.scale =
        detail::read_unsigned_le<std::uint8_t>(packed_data, offset);
    result.size = detail::decimal_quantity_to_double(result.exact_size);
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

  static std::uint64_t size_coefficient_at(const void* packed_data) {
    return detail::read_uint64_le_at(packed_data, packed_size_offset);
  }

  static std::uint8_t size_scale_at(const void* packed_data) {
    const auto* bytes = static_cast<const std::uint8_t*>(packed_data);
    return bytes[packed_size_scale_offset];
  }

  static double size_at(const void* packed_data) {
    return detail::decimal_quantity_to_double(
        detail::DecimalQuantity{
            size_coefficient_at(packed_data),
            size_scale_at(packed_data)});
  }

  static bool condition_at(const void* packed_data, std::size_t condition_code) {
    if (condition_code >= 96) {
      return false;
    }
    const auto* bytes = static_cast<const std::uint8_t*>(packed_data);
    return (bytes[condition_code / 8] &
            static_cast<std::uint8_t>(1U << (condition_code % 8))) != 0;
  }

  static bool updates_high_low_at(const void* packed_data) {
    return !detail::stock_trade_high_low_exclusion_mask.intersects_packed(
        packed_data);
  }

  static bool updates_open_close_at(const void* packed_data) {
    return !detail::stock_trade_open_close_exclusion_mask.intersects_packed(
        packed_data);
  }

  static bool updates_volume_at(const void* packed_data) {
    return !detail::stock_trade_volume_exclusion_mask.intersects_packed(
        packed_data);
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
    detail::write_unsigned_le(output, offset, exact_size.coefficient);
    detail::write_unsigned_le(output, offset, exact_size.scale);
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
           exact_size == other.exact_size &&
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
              detail::bit_indices_frozenset(
                  conditions,
                  detail::ConditionSetKind::stock_trade));
        });
  }

  bool updates_high_low() const {
    return detail::stock_trade_updates_high_low(conditions);
  }

  bool updates_open_close() const {
    return detail::stock_trade_updates_open_close(conditions);
  }

  bool updates_volume() const {
    return detail::stock_trade_updates_volume(conditions);
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
        [&] { return detail::double_object_new_ref(size); });
  }

  nanobind::object decimal_size_object() const {
    return detail::cached_python_object(
        object_cache_,
        decimal_size_attribute,
        [&] {
          return detail::string_object_new_ref(
              detail::decimal_quantity_string(exact_size));
        });
  }

  nanobind::object size_coefficient_object() const {
    return detail::cached_python_object(
        object_cache_,
        size_coefficient_attribute,
        [&] { return detail::uint64_object_new_ref(exact_size.coefficient); });
  }

  nanobind::object size_scale_object() const {
    return detail::cached_python_object(
        object_cache_,
        size_scale_attribute,
        [&] { return detail::uint64_object_new_ref(exact_size.scale); });
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
    detail::hash_combine(seed, exact_size.coefficient);
    detail::hash_combine(seed, exact_size.scale);
    detail::hash_combine(seed, tape);
    detail::hash_combine(seed, trf_id);
    detail::hash_combine(seed, trf_timestamp);
    return seed;
  }

  std::string repr() const {
    std::ostringstream out;
    out << "StockTrade("
        << "ticker='" << ticker << "', "
        << "conditions="
        << detail::bit_indices_repr(
               conditions,
               detail::ConditionSetKind::stock_trade)
        << ", "
        << "correction=" << correction << ", "
        << "exchange=" << static_cast<unsigned>(exchange) << ", "
        << "id=" << id << ", "
        << "participant_timestamp=" << participant_timestamp << ", "
        << "price=" << price << ", "
        << "sequence_number=" << sequence_number << ", "
        << "sip_timestamp=" << sip_timestamp << ", "
        << "size=" << detail::decimal_quantity_string(exact_size) << ", "
        << "tape=" << tape << ", "
        << "trf_id=" << trf_id << ", "
        << "trf_timestamp=" << trf_timestamp << ")";
    return out.str();
  }
};

struct CryptoTrade {
  static constexpr std::size_t packed_size = 46;
  static constexpr std::size_t packed_participant_timestamp_offset = 0;
  static constexpr std::size_t packed_price_offset = 16;
  static constexpr std::size_t packed_size_offset = 24;
  using PackedData = detail::PackedBuffer<packed_size>;
  enum AttributeIndex : std::size_t {
    ticker_attribute,
    conditions_attribute,
    exchange_attribute,
    id_attribute,
    participant_timestamp_attribute,
    price_attribute,
    size_attribute,
    attribute_count,
  };

  std::string ticker;
  std::bitset<96> conditions;
  double price = 0.0;
  double size = 0.0;
  std::uint64_t id = 0;
  std::uint64_t participant_timestamp = 0;
  std::uint16_t exchange = 0;
  mutable std::unique_ptr<detail::LazyPythonObjectCache<attribute_count>> object_cache_;

  CryptoTrade() = default;

  CryptoTrade(const CryptoTrade& other)
      : ticker(other.ticker),
        conditions(other.conditions),
        price(other.price),
        size(other.size),
        id(other.id),
        participant_timestamp(other.participant_timestamp),
        exchange(other.exchange) {}

  CryptoTrade& operator=(const CryptoTrade& other) {
    if (this == &other) {
      return *this;
    }

    ticker = other.ticker;
    conditions = other.conditions;
    price = other.price;
    size = other.size;
    id = other.id;
    participant_timestamp = other.participant_timestamp;
    exchange = other.exchange;
    object_cache_.reset();
    return *this;
  }

  CryptoTrade(CryptoTrade&&) noexcept = default;
  CryptoTrade& operator=(CryptoTrade&&) noexcept = default;

  CryptoTrade(std::string_view packed_data, std::string_view ticker_value) {
    *this = from_packed(packed_data, ticker_value);
  }

  CryptoTrade(const char* packed_data, std::string_view ticker_value)
      : CryptoTrade(std::string_view(packed_data, packed_size), ticker_value) {}

  template <typename Specialization>
  static CryptoTrade from_fields(const std::vector<std::string>& fields) {
    detail::require_field_count("CryptoTrade", fields.size(), 7);
    CryptoTrade result;
    result.ticker = fields[0];
    result.conditions = Specialization::template parse_bitset<96>(fields[1], "conditions");
    result.exchange =
        Specialization::template parse_integer<std::uint16_t>(fields[2], "exchange");
    result.id = Specialization::template parse_integer<std::uint64_t>(fields[3], "id");
    result.participant_timestamp = Specialization::template parse_integer<std::uint64_t>(
        fields[4],
        "participant_timestamp");
    result.price = Specialization::parse_double(fields[5], "price");
    result.size = Specialization::parse_double(fields[6], "size");
    return result;
  }

  static CryptoTrade from_packed(std::string_view packed_data) {
    detail::require_packed_size("CryptoTrade", packed_data.size(), packed_size);
    CryptoTrade result;
    std::size_t offset = 0;
    result.participant_timestamp =
        detail::read_unsigned_le<std::uint64_t>(packed_data, offset);
    result.id = detail::read_unsigned_le<std::uint64_t>(packed_data, offset);
    result.price = detail::read_double_le(packed_data, offset);
    result.size = detail::read_double_le(packed_data, offset);
    result.conditions = detail::read_bitset_le<96>(packed_data, offset);
    result.exchange = detail::read_unsigned_le<std::uint16_t>(packed_data, offset);
    return result;
  }

  static CryptoTrade from_packed(
      std::string_view packed_data,
      std::string_view ticker_value) {
    CryptoTrade result = from_packed(packed_data);
    result.ticker.assign(ticker_value);
    return result;
  }

  static CryptoTrade from_packed_data(
      const char* packed_data,
      std::string_view ticker_value) {
    return from_packed(std::string_view(packed_data, packed_size), ticker_value);
  }

  static std::uint64_t participant_timestamp_at(const void* packed_data) {
    return detail::read_uint64_le_at(
        packed_data,
        packed_participant_timestamp_offset);
  }

  static double price_at(const void* packed_data) {
    return detail::read_double_le_at(packed_data, packed_price_offset);
  }

  static double size_at(const void* packed_data) {
    return detail::read_double_le_at(packed_data, packed_size_offset);
  }

  PackedData pack() const {
    PackedData output{};
    std::size_t offset = 0;
    detail::write_unsigned_le(output, offset, participant_timestamp);
    detail::write_unsigned_le(output, offset, id);
    detail::write_double_le(output, offset, price);
    detail::write_double_le(output, offset, size);
    detail::write_bitset_le(output, offset, conditions);
    detail::write_unsigned_le(output, offset, exchange);
    return output;
  }

  nanobind::bytes packed_bytes() const {
    return detail::packed_bytes(pack());
  }

  bool operator==(const CryptoTrade& other) const {
    return ticker == other.ticker &&
           conditions == other.conditions &&
           exchange == other.exchange &&
           id == other.id &&
           participant_timestamp == other.participant_timestamp &&
           price == other.price &&
           size == other.size;
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
              detail::bit_indices_frozenset(
                  conditions,
                  detail::ConditionSetKind::raw_indices));
        });
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

  nanobind::object size_object() const {
    return detail::cached_python_object(
        object_cache_,
        size_attribute,
        [&] { return detail::double_object_new_ref(size); });
  }

  nanobind::list python_fields() const {
    nanobind::list values;
    values.append(ticker_object());
    values.append(conditions_object());
    values.append(exchange_object());
    values.append(id_object());
    values.append(participant_timestamp_object());
    values.append(price_object());
    values.append(size_object());
    return values;
  }

  std::size_t hash_value() const {
    std::size_t seed = 0;
    detail::hash_combine(seed, ticker);
    detail::hash_combine(seed, conditions);
    detail::hash_combine(seed, exchange);
    detail::hash_combine(seed, id);
    detail::hash_combine(seed, participant_timestamp);
    detail::hash_combine(seed, price);
    detail::hash_combine(seed, size);
    return seed;
  }

  std::string repr() const {
    std::ostringstream out;
    out << "CryptoTrade("
        << "ticker='" << ticker << "', "
        << "conditions="
        << detail::bit_indices_repr(
               conditions,
               detail::ConditionSetKind::raw_indices)
        << ", "
        << "exchange=" << exchange << ", "
        << "id=" << id << ", "
        << "participant_timestamp=" << participant_timestamp << ", "
        << "price=" << price << ", "
        << "size=" << size << ")";
    return out.str();
  }
};

struct OptionTrade {
  static constexpr std::size_t packed_size = 32;
  static constexpr std::size_t packed_sip_timestamp_offset = 0;
  static constexpr std::size_t packed_price_offset = 8;
  static constexpr std::size_t packed_size_offset = 16;
  using PackedData = detail::PackedBuffer<packed_size>;

  enum AttributeIndex : std::size_t {
    root_attribute,
    expiration_attribute,
    right_attribute,
    strike_attribute,
    conditions_attribute,
    correction_attribute,
    exchange_attribute,
    price_attribute,
    sip_timestamp_attribute,
    size_attribute,
    attribute_count,
  };

  std::string root;
  std::string expiration;
  detail::OptionConditionBits conditions;
  double price = 0.0;
  double strike = 0.0;
  std::uint64_t sip_timestamp = 0;
  std::uint32_t strike_millis = 0;
  std::uint32_t size = 0;
  std::int32_t correction = 0;
  std::uint16_t exchange = 0;
  char right = '\0';
  mutable std::unique_ptr<detail::LazyPythonObjectCache<attribute_count>> object_cache_;

  OptionTrade() = default;

  OptionTrade(const OptionTrade& other)
      : root(other.root),
        expiration(other.expiration),
        conditions(other.conditions),
        price(other.price),
        strike(other.strike),
        sip_timestamp(other.sip_timestamp),
        strike_millis(other.strike_millis),
        size(other.size),
        correction(other.correction),
        exchange(other.exchange),
        right(other.right) {}

  OptionTrade& operator=(const OptionTrade& other) {
    if (this == &other) {
      return *this;
    }

    root = other.root;
    expiration = other.expiration;
    conditions = other.conditions;
    price = other.price;
    strike = other.strike;
    sip_timestamp = other.sip_timestamp;
    strike_millis = other.strike_millis;
    size = other.size;
    correction = other.correction;
    exchange = other.exchange;
    right = other.right;
    object_cache_.reset();
    return *this;
  }

  OptionTrade(OptionTrade&&) noexcept = default;
  OptionTrade& operator=(OptionTrade&&) noexcept = default;

  template <typename Specialization>
  static OptionTrade from_fields(const std::vector<std::string>& fields) {
    detail::require_field_count("OptionTrade", fields.size(), 7);
    OptionTrade result;
    result.assign_symbol(fields[0]);
    result.conditions = detail::parse_option_condition_bits(fields[1], "conditions");
    result.correction =
        Specialization::template parse_integer<std::int32_t>(fields[2], "correction");
    result.exchange =
        Specialization::template parse_integer<std::uint16_t>(fields[3], "exchange");
    result.price = Specialization::parse_double(fields[4], "price");
    result.sip_timestamp =
        Specialization::template parse_integer<std::uint64_t>(fields[5], "sip_timestamp");
    result.size =
        Specialization::template parse_integer<std::uint32_t>(fields[6], "size");
    return result;
  }

  void assign_symbol(std::string_view ticker) {
    const detail::OptionSymbolParts symbol = detail::parse_option_symbol(ticker);
    root = symbol.root;
    expiration = symbol.expiration;
    right = symbol.right;
    strike_millis = symbol.strike_millis;
    strike = symbol.strike;
  }

  void assign_contract_key(std::string_view key) {
    const detail::OptionSymbolParts symbol = detail::parse_option_contract_key(key);
    root = symbol.root;
    expiration = symbol.expiration;
    right = symbol.right;
    strike_millis = symbol.strike_millis;
    strike = symbol.strike;
  }

  static OptionTrade from_packed(std::string_view packed_data) {
    detail::require_packed_size("OptionTrade", packed_data.size(), packed_size);
    OptionTrade result;
    std::size_t offset = 0;
    result.sip_timestamp = detail::read_unsigned_le<std::uint64_t>(packed_data, offset);
    result.price = detail::read_double_le(packed_data, offset);
    result.size = detail::read_unsigned_le<std::uint32_t>(packed_data, offset);
    result.correction = detail::read_int32_le(packed_data, offset);
    result.exchange = detail::read_unsigned_le<std::uint16_t>(packed_data, offset);
    for (std::uint8_t& byte : result.conditions.bytes) {
      byte = static_cast<std::uint8_t>(packed_data[offset++]);
    }
    return result;
  }

  static OptionTrade from_packed(
      std::string_view packed_data,
      std::string_view contract_key) {
    OptionTrade result = from_packed(packed_data);
    result.assign_contract_key(contract_key);
    return result;
  }

  static OptionTrade from_packed_data(
      const char* packed_data,
      std::string_view contract_key) {
    return from_packed(std::string_view(packed_data, packed_size), contract_key);
  }

  static std::uint64_t sip_timestamp_at(const void* packed_data) {
    return detail::read_uint64_le_at(packed_data, packed_sip_timestamp_offset);
  }

  static double price_at(const void* packed_data) {
    return detail::read_double_le_at(packed_data, packed_price_offset);
  }

  static std::uint32_t size_at(const void* packed_data) {
    return detail::read_uint32_le_at(packed_data, packed_size_offset);
  }

  PackedData pack() const {
    PackedData output{};
    std::size_t offset = 0;
    detail::write_unsigned_le(output, offset, sip_timestamp);
    detail::write_double_le(output, offset, price);
    detail::write_unsigned_le(output, offset, size);
    detail::write_int32_le(output, offset, correction);
    detail::write_unsigned_le(output, offset, exchange);
    for (const std::uint8_t byte : conditions.bytes) {
      output[offset++] = byte;
    }
    return output;
  }

  nanobind::bytes packed_bytes() const {
    return detail::packed_bytes(pack());
  }

  bool operator==(const OptionTrade& other) const {
    return root == other.root &&
           expiration == other.expiration &&
           right == other.right &&
           strike_millis == other.strike_millis &&
           conditions == other.conditions &&
           correction == other.correction &&
           exchange == other.exchange &&
           price == other.price &&
           sip_timestamp == other.sip_timestamp &&
           size == other.size;
  }

  nanobind::object root_object() const {
    return detail::cached_python_object(
        object_cache_,
        root_attribute,
        [&] { return detail::string_object_new_ref(root); });
  }

  nanobind::object expiration_object() const {
    return detail::cached_python_object(
        object_cache_,
        expiration_attribute,
        [&] { return detail::string_object_new_ref(expiration); });
  }

  nanobind::object right_object() const {
    return detail::cached_python_object(
        object_cache_,
        right_attribute,
        [&] { return PyUnicode_FromStringAndSize(&right, 1); });
  }

  nanobind::object strike_object() const {
    return detail::cached_python_object(
        object_cache_,
        strike_attribute,
        [&] { return detail::double_object_new_ref(strike); });
  }

  nanobind::object conditions_object() const {
    return detail::cached_python_object(
        object_cache_,
        conditions_attribute,
        [&] {
          return detail::object_cache_new_ref(
              detail::option_conditions_frozenset(conditions));
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

  nanobind::object price_object() const {
    return detail::cached_python_object(
        object_cache_,
        price_attribute,
        [&] { return detail::double_object_new_ref(price); });
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
        [&] { return detail::uint64_object_new_ref(size); });
  }

  nanobind::list python_fields() const {
    nanobind::list values;
    values.append(root_object());
    values.append(expiration_object());
    values.append(right_object());
    values.append(strike_object());
    values.append(conditions_object());
    values.append(correction_object());
    values.append(exchange_object());
    values.append(price_object());
    values.append(sip_timestamp_object());
    values.append(size_object());
    return values;
  }

  std::size_t hash_value() const {
    std::size_t seed = 0;
    detail::hash_combine(seed, root);
    detail::hash_combine(seed, expiration);
    detail::hash_combine(seed, right);
    detail::hash_combine(seed, strike_millis);
    detail::hash_combine(seed, detail::option_condition_hash(conditions));
    detail::hash_combine(seed, correction);
    detail::hash_combine(seed, exchange);
    detail::hash_combine(seed, price);
    detail::hash_combine(seed, sip_timestamp);
    detail::hash_combine(seed, size);
    return seed;
  }

  std::string repr() const {
    std::ostringstream out;
    out << "OptionTrade("
        << "root='" << root << "', "
        << "expiration='" << expiration << "', "
        << "right='" << right << "', "
        << "strike=" << strike << ", "
        << "conditions=" << detail::option_conditions_repr(conditions) << ", "
        << "correction=" << correction << ", "
        << "exchange=" << exchange << ", "
        << "price=" << price << ", "
        << "sip_timestamp=" << sip_timestamp << ", "
        << "size=" << size << ")";
    return out.str();
  }
};

struct OptionQuote {
  static constexpr std::size_t packed_size = 44;
  static constexpr std::size_t packed_sip_timestamp_offset = 0;
  static constexpr std::size_t packed_sequence_number_offset = 8;
  static constexpr std::size_t packed_ask_price_offset = 16;
  static constexpr std::size_t packed_bid_price_offset = 24;
  static constexpr std::size_t packed_ask_size_offset = 32;
  static constexpr std::size_t packed_bid_size_offset = 36;
  using PackedData = detail::PackedBuffer<packed_size>;

  enum AttributeIndex : std::size_t {
    root_attribute,
    expiration_attribute,
    right_attribute,
    strike_attribute,
    ask_exchange_attribute,
    ask_price_attribute,
    ask_size_attribute,
    bid_exchange_attribute,
    bid_price_attribute,
    bid_size_attribute,
    sequence_number_attribute,
    sip_timestamp_attribute,
    attribute_count,
  };

  std::string root;
  std::string expiration;
  double ask_price = std::numeric_limits<double>::quiet_NaN();
  double bid_price = std::numeric_limits<double>::quiet_NaN();
  double strike = 0.0;
  std::uint64_t sequence_number = 0;
  std::uint64_t sip_timestamp = 0;
  std::uint32_t strike_millis = 0;
  std::uint32_t ask_size = 0;
  std::uint32_t bid_size = 0;
  std::uint16_t ask_exchange = 0;
  std::uint16_t bid_exchange = 0;
  char right = '\0';
  mutable std::unique_ptr<detail::LazyPythonObjectCache<attribute_count>> object_cache_;

  OptionQuote() = default;

  OptionQuote(const OptionQuote& other)
      : root(other.root),
        expiration(other.expiration),
        ask_price(other.ask_price),
        bid_price(other.bid_price),
        strike(other.strike),
        sequence_number(other.sequence_number),
        sip_timestamp(other.sip_timestamp),
        strike_millis(other.strike_millis),
        ask_size(other.ask_size),
        bid_size(other.bid_size),
        ask_exchange(other.ask_exchange),
        bid_exchange(other.bid_exchange),
        right(other.right) {}

  OptionQuote& operator=(const OptionQuote& other) {
    if (this == &other) {
      return *this;
    }

    root = other.root;
    expiration = other.expiration;
    ask_price = other.ask_price;
    bid_price = other.bid_price;
    strike = other.strike;
    sequence_number = other.sequence_number;
    sip_timestamp = other.sip_timestamp;
    strike_millis = other.strike_millis;
    ask_size = other.ask_size;
    bid_size = other.bid_size;
    ask_exchange = other.ask_exchange;
    bid_exchange = other.bid_exchange;
    right = other.right;
    object_cache_.reset();
    return *this;
  }

  OptionQuote(OptionQuote&&) noexcept = default;
  OptionQuote& operator=(OptionQuote&&) noexcept = default;

  template <typename Specialization>
  static OptionQuote from_fields(const std::vector<std::string>& fields) {
    detail::require_field_count("OptionQuote", fields.size(), 9);
    OptionQuote result;
    result.assign_symbol(fields[0]);
    result.ask_exchange =
        Specialization::template parse_integer<std::uint16_t>(fields[1], "ask_exchange");
    result.ask_price = detail::parse_nullable_double(fields[2], "ask_price");
    result.ask_size =
        Specialization::template parse_integer<std::uint32_t>(fields[3], "ask_size");
    result.bid_exchange =
        Specialization::template parse_integer<std::uint16_t>(fields[4], "bid_exchange");
    result.bid_price = detail::parse_nullable_double(fields[5], "bid_price");
    result.bid_size =
        Specialization::template parse_integer<std::uint32_t>(fields[6], "bid_size");
    result.sequence_number =
        Specialization::template parse_integer<std::uint64_t>(fields[7], "sequence_number");
    result.sip_timestamp =
        Specialization::template parse_integer<std::uint64_t>(fields[8], "sip_timestamp");
    return result;
  }

  void assign_symbol(std::string_view ticker) {
    const detail::OptionSymbolParts symbol = detail::parse_option_symbol(ticker);
    root = symbol.root;
    expiration = symbol.expiration;
    right = symbol.right;
    strike_millis = symbol.strike_millis;
    strike = symbol.strike;
  }

  void assign_contract_key(std::string_view key) {
    const detail::OptionSymbolParts symbol = detail::parse_option_contract_key(key);
    root = symbol.root;
    expiration = symbol.expiration;
    right = symbol.right;
    strike_millis = symbol.strike_millis;
    strike = symbol.strike;
  }

  static OptionQuote from_packed(std::string_view packed_data) {
    detail::require_packed_size("OptionQuote", packed_data.size(), packed_size);
    OptionQuote result;
    std::size_t offset = 0;
    result.sip_timestamp = detail::read_unsigned_le<std::uint64_t>(packed_data, offset);
    result.sequence_number = detail::read_unsigned_le<std::uint64_t>(packed_data, offset);
    result.ask_price = detail::read_double_le(packed_data, offset);
    result.bid_price = detail::read_double_le(packed_data, offset);
    result.ask_size = detail::read_unsigned_le<std::uint32_t>(packed_data, offset);
    result.bid_size = detail::read_unsigned_le<std::uint32_t>(packed_data, offset);
    result.ask_exchange = detail::read_unsigned_le<std::uint16_t>(packed_data, offset);
    result.bid_exchange = detail::read_unsigned_le<std::uint16_t>(packed_data, offset);
    return result;
  }

  static OptionQuote from_packed(
      std::string_view packed_data,
      std::string_view contract_key) {
    OptionQuote result = from_packed(packed_data);
    result.assign_contract_key(contract_key);
    return result;
  }

  static OptionQuote from_packed_data(
      const char* packed_data,
      std::string_view contract_key) {
    return from_packed(std::string_view(packed_data, packed_size), contract_key);
  }

  static std::uint64_t sip_timestamp_at(const void* packed_data) {
    return detail::read_uint64_le_at(packed_data, packed_sip_timestamp_offset);
  }

  static double ask_price_at(const void* packed_data) {
    return detail::read_double_le_at(packed_data, packed_ask_price_offset);
  }

  static double bid_price_at(const void* packed_data) {
    return detail::read_double_le_at(packed_data, packed_bid_price_offset);
  }

  static std::uint32_t ask_size_at(const void* packed_data) {
    return detail::read_uint32_le_at(packed_data, packed_ask_size_offset);
  }

  static std::uint32_t bid_size_at(const void* packed_data) {
    return detail::read_uint32_le_at(packed_data, packed_bid_size_offset);
  }

  PackedData pack() const {
    PackedData output{};
    std::size_t offset = 0;
    detail::write_unsigned_le(output, offset, sip_timestamp);
    detail::write_unsigned_le(output, offset, sequence_number);
    detail::write_double_le(output, offset, ask_price);
    detail::write_double_le(output, offset, bid_price);
    detail::write_unsigned_le(output, offset, ask_size);
    detail::write_unsigned_le(output, offset, bid_size);
    detail::write_unsigned_le(output, offset, ask_exchange);
    detail::write_unsigned_le(output, offset, bid_exchange);
    return output;
  }

  nanobind::bytes packed_bytes() const {
    return detail::packed_bytes(pack());
  }

  bool operator==(const OptionQuote& other) const {
    const bool ask_prices_equal =
        ask_price == other.ask_price || (std::isnan(ask_price) && std::isnan(other.ask_price));
    const bool bid_prices_equal =
        bid_price == other.bid_price || (std::isnan(bid_price) && std::isnan(other.bid_price));
    return root == other.root &&
           expiration == other.expiration &&
           right == other.right &&
           strike_millis == other.strike_millis &&
           ask_exchange == other.ask_exchange &&
           ask_prices_equal &&
           ask_size == other.ask_size &&
           bid_exchange == other.bid_exchange &&
           bid_prices_equal &&
           bid_size == other.bid_size &&
           sequence_number == other.sequence_number &&
           sip_timestamp == other.sip_timestamp;
  }

  nanobind::object root_object() const {
    return detail::cached_python_object(
        object_cache_,
        root_attribute,
        [&] { return detail::string_object_new_ref(root); });
  }

  nanobind::object expiration_object() const {
    return detail::cached_python_object(
        object_cache_,
        expiration_attribute,
        [&] { return detail::string_object_new_ref(expiration); });
  }

  nanobind::object right_object() const {
    return detail::cached_python_object(
        object_cache_,
        right_attribute,
        [&] { return PyUnicode_FromStringAndSize(&right, 1); });
  }

  nanobind::object strike_object() const {
    return detail::cached_python_object(
        object_cache_,
        strike_attribute,
        [&] { return detail::double_object_new_ref(strike); });
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

  nanobind::list python_fields() const {
    nanobind::list values;
    values.append(root_object());
    values.append(expiration_object());
    values.append(right_object());
    values.append(strike_object());
    values.append(ask_exchange_object());
    values.append(ask_price_object());
    values.append(ask_size_object());
    values.append(bid_exchange_object());
    values.append(bid_price_object());
    values.append(bid_size_object());
    values.append(sequence_number_object());
    values.append(sip_timestamp_object());
    return values;
  }

  std::size_t hash_value() const {
    std::size_t seed = 0;
    detail::hash_combine(seed, root);
    detail::hash_combine(seed, expiration);
    detail::hash_combine(seed, right);
    detail::hash_combine(seed, strike_millis);
    detail::hash_combine(seed, ask_exchange);
    detail::hash_combine(seed, ask_price);
    detail::hash_combine(seed, ask_size);
    detail::hash_combine(seed, bid_exchange);
    detail::hash_combine(seed, bid_price);
    detail::hash_combine(seed, bid_size);
    detail::hash_combine(seed, sequence_number);
    detail::hash_combine(seed, sip_timestamp);
    return seed;
  }

  std::string repr() const {
    std::ostringstream out;
    out << "OptionQuote("
        << "root='" << root << "', "
        << "expiration='" << expiration << "', "
        << "right='" << right << "', "
        << "strike=" << strike << ", "
        << "ask_exchange=" << ask_exchange << ", "
        << "ask_price=" << ask_price << ", "
        << "ask_size=" << ask_size << ", "
        << "bid_exchange=" << bid_exchange << ", "
        << "bid_price=" << bid_price << ", "
        << "bid_size=" << bid_size << ", "
        << "sequence_number=" << sequence_number << ", "
        << "sip_timestamp=" << sip_timestamp << ")";
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
              detail::bit_indices_frozenset(
                  conditions,
                  detail::ConditionSetKind::stock_quote));
        });
  }

  nanobind::object indicators_object() const {
    return detail::cached_python_object(
        object_cache_,
        indicators_attribute,
        [&] {
          return detail::object_cache_new_ref(
              detail::bit_indices_frozenset(
                  indicators,
                  detail::ConditionSetKind::stock_quote));
        });
  }

  bool updates_high_low() const {
    return detail::stock_quote_updates_high_low(conditions | indicators);
  }

  bool updates_open_close() const {
    return detail::stock_quote_updates_open_close(conditions | indicators);
  }

  bool updates_volume() const {
    return detail::stock_quote_updates_volume(conditions | indicators);
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
        << "conditions="
        << detail::bit_indices_repr(
               conditions,
               detail::ConditionSetKind::stock_quote)
        << ", "
        << "indicators="
        << detail::bit_indices_repr(
               indicators,
               detail::ConditionSetKind::stock_quote)
        << ", "
        << "participant_timestamp=" << participant_timestamp << ", "
        << "sequence_number=" << sequence_number << ", "
        << "sip_timestamp=" << sip_timestamp << ", "
        << "tape=" << static_cast<unsigned>(tape) << ", "
        << "trf_timestamp=" << trf_timestamp << ")";
    return out.str();
  }
};

struct FuturesTrade {
  static constexpr std::size_t packed_size = 38;
  static constexpr std::size_t packed_timestamp_offset = 0;
  static constexpr std::size_t packed_price_offset = 16;
  static constexpr std::size_t packed_size_offset = 28;
  using PackedData = detail::PackedBuffer<packed_size>;
  enum AttributeIndex : std::size_t {
    ticker_attribute,
    timestamp_attribute,
    sequence_number_attribute,
    report_sequence_attribute,
    price_attribute,
    size_attribute,
    correction_attribute,
    exchange_attribute,
    attribute_count,
  };

  std::string ticker;
  double price = 0.0;
  std::uint64_t timestamp = 0;
  std::uint64_t sequence_number = 0;
  std::uint32_t report_sequence = 0;
  std::uint32_t size = 0;
  std::int32_t correction = 0;
  std::uint16_t exchange = 0;
  mutable std::unique_ptr<detail::LazyPythonObjectCache<attribute_count>> object_cache_;

  FuturesTrade() = default;

  FuturesTrade(const FuturesTrade& other)
      : ticker(other.ticker),
        price(other.price),
        timestamp(other.timestamp),
        sequence_number(other.sequence_number),
        report_sequence(other.report_sequence),
        size(other.size),
        correction(other.correction),
        exchange(other.exchange) {}

  FuturesTrade& operator=(const FuturesTrade& other) {
    if (this == &other) {
      return *this;
    }
    ticker = other.ticker;
    price = other.price;
    timestamp = other.timestamp;
    sequence_number = other.sequence_number;
    report_sequence = other.report_sequence;
    size = other.size;
    correction = other.correction;
    exchange = other.exchange;
    object_cache_.reset();
    return *this;
  }

  FuturesTrade(FuturesTrade&&) noexcept = default;
  FuturesTrade& operator=(FuturesTrade&&) noexcept = default;

  FuturesTrade(std::string_view packed_data, std::string_view ticker_value) {
    *this = from_packed(packed_data, ticker_value);
  }

  FuturesTrade(const char* packed_data, std::string_view ticker_value)
      : FuturesTrade(std::string_view(packed_data, packed_size), ticker_value) {}

  template <typename Specialization>
  static FuturesTrade from_fields(const std::vector<std::string>& fields) {
    detail::require_field_count("FuturesTrade", fields.size(), 9);
    FuturesTrade result;
    result.ticker = fields[0];
    result.timestamp =
        Specialization::template parse_integer<std::uint64_t>(fields[1], "timestamp");
    result.sequence_number =
        Specialization::template parse_integer<std::uint64_t>(fields[2], "sequence_number");
    result.report_sequence =
        Specialization::template parse_integer<std::uint32_t>(fields[3], "report_sequence");
    result.price = Specialization::parse_double(fields[4], "price");
    result.size =
        Specialization::template parse_integer<std::uint32_t>(fields[5], "size");
    result.correction =
        Specialization::template parse_integer<std::int32_t>(fields[6], "correction");
    result.exchange =
        Specialization::template parse_integer<std::uint16_t>(fields[7], "exchange");
    return result;
  }

  static FuturesTrade from_packed(std::string_view packed_data) {
    detail::require_packed_size("FuturesTrade", packed_data.size(), packed_size);
    FuturesTrade result;
    std::size_t offset = 0;
    result.timestamp = detail::read_unsigned_le<std::uint64_t>(packed_data, offset);
    result.sequence_number = detail::read_unsigned_le<std::uint64_t>(packed_data, offset);
    result.price = detail::read_double_le(packed_data, offset);
    result.report_sequence = detail::read_unsigned_le<std::uint32_t>(packed_data, offset);
    result.size = detail::read_unsigned_le<std::uint32_t>(packed_data, offset);
    result.correction = detail::read_int32_le(packed_data, offset);
    result.exchange = detail::read_unsigned_le<std::uint16_t>(packed_data, offset);
    return result;
  }

  static FuturesTrade from_packed(std::string_view packed_data, std::string_view ticker_value) {
    FuturesTrade result = from_packed(packed_data);
    result.ticker.assign(ticker_value);
    return result;
  }

  static FuturesTrade from_packed_data(const char* packed_data, std::string_view ticker_value) {
    return from_packed(std::string_view(packed_data, packed_size), ticker_value);
  }

  static std::uint64_t timestamp_at(const void* packed_data) {
    return detail::read_uint64_le_at(packed_data, packed_timestamp_offset);
  }

  static double price_at(const void* packed_data) {
    return detail::read_double_le_at(packed_data, packed_price_offset);
  }

  static std::uint32_t size_at(const void* packed_data) {
    return detail::read_uint32_le_at(packed_data, packed_size_offset);
  }

  PackedData pack() const {
    PackedData output{};
    std::size_t offset = 0;
    detail::write_unsigned_le(output, offset, timestamp);
    detail::write_unsigned_le(output, offset, sequence_number);
    detail::write_double_le(output, offset, price);
    detail::write_unsigned_le(output, offset, report_sequence);
    detail::write_unsigned_le(output, offset, size);
    detail::write_int32_le(output, offset, correction);
    detail::write_unsigned_le(output, offset, exchange);
    return output;
  }

  nanobind::bytes packed_bytes() const {
    return detail::packed_bytes(pack());
  }

  nanobind::object ticker_object() const {
    return detail::cached_python_object(
        object_cache_,
        ticker_attribute,
        [&] { return detail::string_object_new_ref(ticker); });
  }

  nanobind::object timestamp_object() const {
    return detail::cached_python_object(
        object_cache_,
        timestamp_attribute,
        [&] { return detail::uint64_object_new_ref(timestamp); });
  }

  nanobind::object sequence_number_object() const {
    return detail::cached_python_object(
        object_cache_,
        sequence_number_attribute,
        [&] { return detail::uint64_object_new_ref(sequence_number); });
  }

  nanobind::object report_sequence_object() const {
    return detail::cached_python_object(
        object_cache_,
        report_sequence_attribute,
        [&] { return detail::uint64_object_new_ref(report_sequence); });
  }

  nanobind::object price_object() const {
    return detail::cached_python_object(
        object_cache_,
        price_attribute,
        [&] { return detail::double_object_new_ref(price); });
  }

  nanobind::object size_object() const {
    return detail::cached_python_object(
        object_cache_,
        size_attribute,
        [&] { return detail::uint64_object_new_ref(size); });
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

  nanobind::list python_fields() const {
    nanobind::list values;
    values.append(ticker_object());
    values.append(timestamp_object());
    values.append(sequence_number_object());
    values.append(report_sequence_object());
    values.append(price_object());
    values.append(size_object());
    values.append(correction_object());
    values.append(exchange_object());
    return values;
  }

  bool operator==(const FuturesTrade& other) const {
    return ticker == other.ticker &&
           timestamp == other.timestamp &&
           sequence_number == other.sequence_number &&
           report_sequence == other.report_sequence &&
           price == other.price &&
           size == other.size &&
           correction == other.correction &&
           exchange == other.exchange;
  }

  std::size_t hash_value() const {
    std::size_t seed = 0;
    detail::hash_combine(seed, ticker);
    detail::hash_combine(seed, timestamp);
    detail::hash_combine(seed, sequence_number);
    detail::hash_combine(seed, report_sequence);
    detail::hash_combine(seed, price);
    detail::hash_combine(seed, size);
    detail::hash_combine(seed, correction);
    detail::hash_combine(seed, exchange);
    return seed;
  }

  std::string repr() const {
    std::ostringstream out;
    out << "FuturesTrade("
        << "ticker='" << ticker << "', "
        << "timestamp=" << timestamp << ", "
        << "sequence_number=" << sequence_number << ", "
        << "report_sequence=" << report_sequence << ", "
        << "price=" << price << ", "
        << "size=" << size << ", "
        << "correction=" << correction << ", "
        << "exchange=" << exchange << ")";
    return out.str();
  }
};

struct FuturesQuote {
  static constexpr std::size_t packed_size = 62;
  static constexpr std::size_t packed_timestamp_offset = 0;
  static constexpr std::size_t packed_ask_price_offset = 32;
  static constexpr std::size_t packed_bid_price_offset = 40;
  static constexpr std::size_t packed_ask_size_offset = 52;
  static constexpr std::size_t packed_bid_size_offset = 56;
  using PackedData = detail::PackedBuffer<packed_size>;
  enum AttributeIndex : std::size_t {
    ticker_attribute,
    timestamp_attribute,
    sequence_number_attribute,
    report_sequence_attribute,
    ask_timestamp_attribute,
    ask_price_attribute,
    ask_size_attribute,
    bid_timestamp_attribute,
    bid_price_attribute,
    bid_size_attribute,
    exchange_attribute,
    attribute_count,
  };

  std::string ticker;
  double ask_price = std::numeric_limits<double>::quiet_NaN();
  double bid_price = std::numeric_limits<double>::quiet_NaN();
  std::uint64_t timestamp = 0;
  std::uint64_t sequence_number = 0;
  std::uint64_t ask_timestamp = 0;
  std::uint64_t bid_timestamp = 0;
  std::uint32_t report_sequence = 0;
  std::uint32_t ask_size = 0;
  std::uint32_t bid_size = 0;
  std::uint16_t exchange = 0;
  mutable std::unique_ptr<detail::LazyPythonObjectCache<attribute_count>> object_cache_;

  FuturesQuote() = default;

  FuturesQuote(const FuturesQuote& other)
      : ticker(other.ticker),
        ask_price(other.ask_price),
        bid_price(other.bid_price),
        timestamp(other.timestamp),
        sequence_number(other.sequence_number),
        ask_timestamp(other.ask_timestamp),
        bid_timestamp(other.bid_timestamp),
        report_sequence(other.report_sequence),
        ask_size(other.ask_size),
        bid_size(other.bid_size),
        exchange(other.exchange) {}

  FuturesQuote& operator=(const FuturesQuote& other) {
    if (this == &other) {
      return *this;
    }
    ticker = other.ticker;
    ask_price = other.ask_price;
    bid_price = other.bid_price;
    timestamp = other.timestamp;
    sequence_number = other.sequence_number;
    ask_timestamp = other.ask_timestamp;
    bid_timestamp = other.bid_timestamp;
    report_sequence = other.report_sequence;
    ask_size = other.ask_size;
    bid_size = other.bid_size;
    exchange = other.exchange;
    object_cache_.reset();
    return *this;
  }

  FuturesQuote(FuturesQuote&&) noexcept = default;
  FuturesQuote& operator=(FuturesQuote&&) noexcept = default;

  FuturesQuote(std::string_view packed_data, std::string_view ticker_value) {
    *this = from_packed(packed_data, ticker_value);
  }

  FuturesQuote(const char* packed_data, std::string_view ticker_value)
      : FuturesQuote(std::string_view(packed_data, packed_size), ticker_value) {}

  template <typename Specialization>
  static FuturesQuote from_fields(const std::vector<std::string>& fields) {
    detail::require_field_count("FuturesQuote", fields.size(), 12);
    FuturesQuote result;
    result.ticker = fields[0];
    result.timestamp =
        Specialization::template parse_integer<std::uint64_t>(fields[1], "timestamp");
    result.sequence_number =
        Specialization::template parse_integer<std::uint64_t>(fields[2], "sequence_number");
    result.report_sequence =
        Specialization::template parse_integer<std::uint32_t>(fields[3], "report_sequence");
    result.ask_timestamp =
        Specialization::template parse_integer<std::uint64_t>(fields[4], "ask_timestamp");
    result.ask_price = detail::parse_nullable_double(fields[5], "ask_price");
    result.ask_size =
        Specialization::template parse_integer<std::uint32_t>(fields[6], "ask_size");
    result.bid_timestamp =
        Specialization::template parse_integer<std::uint64_t>(fields[7], "bid_timestamp");
    result.bid_price = detail::parse_nullable_double(fields[8], "bid_price");
    result.bid_size =
        Specialization::template parse_integer<std::uint32_t>(fields[9], "bid_size");
    result.exchange =
        Specialization::template parse_integer<std::uint16_t>(fields[10], "exchange");
    return result;
  }

  static FuturesQuote from_packed(std::string_view packed_data) {
    detail::require_packed_size("FuturesQuote", packed_data.size(), packed_size);
    FuturesQuote result;
    std::size_t offset = 0;
    result.timestamp = detail::read_unsigned_le<std::uint64_t>(packed_data, offset);
    result.sequence_number = detail::read_unsigned_le<std::uint64_t>(packed_data, offset);
    result.ask_timestamp = detail::read_unsigned_le<std::uint64_t>(packed_data, offset);
    result.bid_timestamp = detail::read_unsigned_le<std::uint64_t>(packed_data, offset);
    result.ask_price = detail::read_double_le(packed_data, offset);
    result.bid_price = detail::read_double_le(packed_data, offset);
    result.report_sequence = detail::read_unsigned_le<std::uint32_t>(packed_data, offset);
    result.ask_size = detail::read_unsigned_le<std::uint32_t>(packed_data, offset);
    result.bid_size = detail::read_unsigned_le<std::uint32_t>(packed_data, offset);
    result.exchange = detail::read_unsigned_le<std::uint16_t>(packed_data, offset);
    return result;
  }

  static FuturesQuote from_packed(std::string_view packed_data, std::string_view ticker_value) {
    FuturesQuote result = from_packed(packed_data);
    result.ticker.assign(ticker_value);
    return result;
  }

  static FuturesQuote from_packed_data(const char* packed_data, std::string_view ticker_value) {
    return from_packed(std::string_view(packed_data, packed_size), ticker_value);
  }

  static std::uint64_t timestamp_at(const void* packed_data) {
    return detail::read_uint64_le_at(packed_data, packed_timestamp_offset);
  }

  static double ask_price_at(const void* packed_data) {
    return detail::read_double_le_at(packed_data, packed_ask_price_offset);
  }

  static double bid_price_at(const void* packed_data) {
    return detail::read_double_le_at(packed_data, packed_bid_price_offset);
  }

  static std::uint32_t ask_size_at(const void* packed_data) {
    return detail::read_uint32_le_at(packed_data, packed_ask_size_offset);
  }

  static std::uint32_t bid_size_at(const void* packed_data) {
    return detail::read_uint32_le_at(packed_data, packed_bid_size_offset);
  }

  PackedData pack() const {
    PackedData output{};
    std::size_t offset = 0;
    detail::write_unsigned_le(output, offset, timestamp);
    detail::write_unsigned_le(output, offset, sequence_number);
    detail::write_unsigned_le(output, offset, ask_timestamp);
    detail::write_unsigned_le(output, offset, bid_timestamp);
    detail::write_double_le(output, offset, ask_price);
    detail::write_double_le(output, offset, bid_price);
    detail::write_unsigned_le(output, offset, report_sequence);
    detail::write_unsigned_le(output, offset, ask_size);
    detail::write_unsigned_le(output, offset, bid_size);
    detail::write_unsigned_le(output, offset, exchange);
    return output;
  }

  nanobind::bytes packed_bytes() const {
    return detail::packed_bytes(pack());
  }

  nanobind::object ticker_object() const {
    return detail::cached_python_object(
        object_cache_,
        ticker_attribute,
        [&] { return detail::string_object_new_ref(ticker); });
  }

  nanobind::object timestamp_object() const {
    return detail::cached_python_object(
        object_cache_,
        timestamp_attribute,
        [&] { return detail::uint64_object_new_ref(timestamp); });
  }

  nanobind::object sequence_number_object() const {
    return detail::cached_python_object(
        object_cache_,
        sequence_number_attribute,
        [&] { return detail::uint64_object_new_ref(sequence_number); });
  }

  nanobind::object report_sequence_object() const {
    return detail::cached_python_object(
        object_cache_,
        report_sequence_attribute,
        [&] { return detail::uint64_object_new_ref(report_sequence); });
  }

  nanobind::object ask_timestamp_object() const {
    return detail::cached_python_object(
        object_cache_,
        ask_timestamp_attribute,
        [&] { return detail::uint64_object_new_ref(ask_timestamp); });
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

  nanobind::object bid_timestamp_object() const {
    return detail::cached_python_object(
        object_cache_,
        bid_timestamp_attribute,
        [&] { return detail::uint64_object_new_ref(bid_timestamp); });
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

  nanobind::object exchange_object() const {
    return detail::cached_python_object(
        object_cache_,
        exchange_attribute,
        [&] { return detail::uint64_object_new_ref(exchange); });
  }

  nanobind::list python_fields() const {
    nanobind::list values;
    values.append(ticker_object());
    values.append(timestamp_object());
    values.append(sequence_number_object());
    values.append(report_sequence_object());
    values.append(ask_timestamp_object());
    values.append(ask_price_object());
    values.append(ask_size_object());
    values.append(bid_timestamp_object());
    values.append(bid_price_object());
    values.append(bid_size_object());
    values.append(exchange_object());
    return values;
  }

  bool operator==(const FuturesQuote& other) const {
    const bool ask_prices_equal =
        ask_price == other.ask_price || (std::isnan(ask_price) && std::isnan(other.ask_price));
    const bool bid_prices_equal =
        bid_price == other.bid_price || (std::isnan(bid_price) && std::isnan(other.bid_price));
    return ticker == other.ticker &&
           timestamp == other.timestamp &&
           sequence_number == other.sequence_number &&
           report_sequence == other.report_sequence &&
           ask_timestamp == other.ask_timestamp &&
           ask_prices_equal &&
           ask_size == other.ask_size &&
           bid_timestamp == other.bid_timestamp &&
           bid_prices_equal &&
           bid_size == other.bid_size &&
           exchange == other.exchange;
  }

  std::size_t hash_value() const {
    std::size_t seed = 0;
    detail::hash_combine(seed, ticker);
    detail::hash_combine(seed, timestamp);
    detail::hash_combine(seed, sequence_number);
    detail::hash_combine(seed, report_sequence);
    detail::hash_combine(seed, ask_timestamp);
    detail::hash_combine(seed, ask_price);
    detail::hash_combine(seed, ask_size);
    detail::hash_combine(seed, bid_timestamp);
    detail::hash_combine(seed, bid_price);
    detail::hash_combine(seed, bid_size);
    detail::hash_combine(seed, exchange);
    return seed;
  }

  std::string repr() const {
    std::ostringstream out;
    out << "FuturesQuote("
        << "ticker='" << ticker << "', "
        << "timestamp=" << timestamp << ", "
        << "sequence_number=" << sequence_number << ", "
        << "report_sequence=" << report_sequence << ", "
        << "ask_timestamp=" << ask_timestamp << ", "
        << "ask_price=" << ask_price << ", "
        << "ask_size=" << ask_size << ", "
        << "bid_timestamp=" << bid_timestamp << ", "
        << "bid_price=" << bid_price << ", "
        << "bid_size=" << bid_size << ", "
        << "exchange=" << exchange << ")";
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
  bool has_high_low = false;
  bool has_open_close = false;
  double open = 0.0;
  double close = 0.0;
  double high = 0.0;
  double low = 0.0;
  double mean = 0.0;
  double m2 = 0.0;
  std::uint64_t count = 0;

  void add(double value) {
    add(value, true, true, true);
  }

  void add(
      double value,
      bool update_high_low,
      bool update_open_close,
      bool update_statistics) {
    if (update_high_low) {
      if (!has_high_low) {
        has_high_low = true;
        high = value;
        low = value;
      } else {
        high = std::max(high, value);
        low = std::min(low, value);
      }
    }

    if (update_open_close) {
      if (!has_open_close) {
        has_open_close = true;
        open = value;
      }
      close = value;
    }

    if (!update_statistics) {
      return;
    }

    ++count;

    const double delta = value - mean;
    mean += delta / static_cast<double>(count);
    const double delta2 = value - mean;
    m2 += delta * delta2;
  }

  void add_open_close_only(double value) {
    if (!has_open_close) {
      has_open_close = true;
      open = value;
    }
    close = value;
  }

  double average() const {
    return count == 0 ? quiet_nan() : mean;
  }

  double stddev() const {
    return count == 0 ? quiet_nan() : std::sqrt(m2 / static_cast<double>(count));
  }

  double change() const {
    return !has_open_close ? quiet_nan() : close - open;
  }

  double range() const {
    return !has_high_low ? quiet_nan() : high - low;
  }

  double return_bps() const {
    if (!has_open_close || open == 0.0) {
      return quiet_nan();
    }
    return ((close / open) - 1.0) * 10'000.0;
  }

  double range_bps() const {
    if (!has_high_low || !has_open_close || open == 0.0) {
      return quiet_nan();
    }
    return ((high - low) / open) * 10'000.0;
  }
};

struct WeightedPriceAggregation {
  long double weighted_sum = 0.0;
  long double weight = 0.0;

  void add(double value, double value_weight) {
    weighted_sum += static_cast<long double>(value) *
                    static_cast<long double>(value_weight);
    weight += value_weight;
  }

  double average() const {
    if (weight == 0.0L) {
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
  double volume = 0.0;
  std::uint64_t window_start = 0;
  std::uint64_t transactions = 0;
  double min_trade_size = 0.0;
  double max_trade_size = 0.0;
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
  double volume = 0.0;
  double min_trade_size = 0.0;
  double max_trade_size = 0.0;
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
    const double row_volume =
        !detail::stock_trade_updates_volume(row.conditions) ||
                !std::isfinite(row.size) || row.size <= 0.0F
            ? 0.0
            : static_cast<double>(row.size);
    add_values(
        row.price,
        row_volume,
        row.sip_timestamp,
        detail::stock_trade_updates_high_low(row.conditions),
        detail::stock_trade_updates_open_close(row.conditions));
  }

  void add_values(
      double value,
      double row_volume,
      std::uint64_t timestamp,
      bool updates_high_low = true,
      bool updates_open_close = true) {
    price.add(value, updates_high_low, updates_open_close, updates_high_low);
    if (row_volume > 0.0) {
      weighted_price.add(value, row_volume);
    }
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
    result.open = price.has_open_close ? price.open : detail::quiet_nan();
    result.close = price.has_open_close ? price.close : detail::quiet_nan();
    result.high = price.has_high_low ? price.high : detail::quiet_nan();
    result.low = price.has_high_low ? price.low : detail::quiet_nan();
    result.avg = price.average();
    result.volume_weighted_avg = weighted_price.average();
    result.volume = volume;
    result.window_start = window_start;
    result.transactions = transactions;
    result.stddev = price.stddev();
    result.dollar_volume = static_cast<double>(
        weighted_price.weight == 0.0L ? 0.0L : weighted_price.weighted_sum);
    result.avg_trade_size =
        transactions == 0
            ? detail::quiet_nan()
            : volume / static_cast<double>(transactions);
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
class CryptoTradeDatabase;
class CurrencyQuoteDatabase;
class OptionTradeDatabase;
class OptionQuoteDatabase;

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
    const double size = RowType::size_at(packed_data);
    state.add_values(
        RowType::price_at(packed_data),
        !RowType::updates_volume_at(packed_data) ||
                !std::isfinite(size) || size <= 0.0F
            ? 0.0
            : static_cast<double>(size),
        timestamp,
        RowType::updates_high_low_at(packed_data),
        RowType::updates_open_close_at(packed_data));
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

struct CryptoTradeDatabaseTraits {
  using row_type = CryptoTrade;
  static constexpr std::string_view record_type = "crypto_trade";

  static std::uint64_t search_timestamp_at(const void* packed_data) {
    return CryptoTrade::participant_timestamp_at(packed_data);
  }
};

struct CurrencyQuoteDatabaseTraits {
  using row_type = CurrencyQuote;
  static constexpr std::string_view record_type = "currency_quote";

  static std::uint64_t search_timestamp_at(const void* packed_data) {
    return CurrencyQuote::participant_timestamp_at(packed_data);
  }
};

struct FuturesTradeDatabaseTraits {
  using row_type = FuturesTrade;
  static constexpr std::string_view record_type = "future_trade";

  static std::uint64_t search_timestamp_at(const void* packed_data) {
    return FuturesTrade::timestamp_at(packed_data);
  }
};

struct FuturesQuoteDatabaseTraits {
  using row_type = FuturesQuote;
  static constexpr std::string_view record_type = "future_quote";

  static std::uint64_t search_timestamp_at(const void* packed_data) {
    return FuturesQuote::timestamp_at(packed_data);
  }
};

struct OptionTradeDatabaseTraits {
  using row_type = OptionTrade;
  static constexpr std::string_view record_type = "option_trade";

  static std::uint64_t search_timestamp_at(const void* packed_data) {
    return OptionTrade::sip_timestamp_at(packed_data);
  }
};

struct OptionQuoteDatabaseTraits {
  using row_type = OptionQuote;
  static constexpr std::string_view record_type = "option_quote";

  static std::uint64_t search_timestamp_at(const void* packed_data) {
    return OptionQuote::sip_timestamp_at(packed_data);
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
      : DatabaseRecordFile(
            std::move(database_path),
            std::move(date),
            std::move(ticker),
            std::string(Traits::record_type)) {}

  DatabaseRecordFile(
      std::filesystem::path database_path,
      std::string date,
      std::string ticker,
      std::string record_type)
      : database_path_(std::move(database_path)),
        date_(std::move(date)),
        ticker_(std::move(ticker)),
        record_type_(std::move(record_type)),
        file_path_(database_path_ / record_type_ / date_ / ticker_),
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
  const std::string& record_type() const { return record_type_; }
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
      std::optional<std::int64_t> galloping = std::nullopt) const {
    if (size_ == 0 || timestamp_at(0) > timestamp) {
      return -1;
    }

    if (galloping) {
      return galloping_index_before_timestamp(timestamp, *galloping);
    }
    return binary_index_before_timestamp(0, size_, timestamp);
  }

  std::int64_t index_after_timestamp(
      std::uint64_t timestamp,
      std::optional<std::int64_t> galloping = std::nullopt) const {
    if (size_ == 0 || timestamp_at(size_ - 1) < timestamp) {
      return -1;
    }

    if (galloping) {
      return galloping_index_after_timestamp(timestamp, *galloping);
    }
    return binary_index_after_timestamp(0, size_, timestamp);
  }

  RowType find_before_participant_timestamp(
      std::uint64_t timestamp,
      std::uint64_t fuzz = 1'000'000'000ULL,
      std::optional<std::int64_t> galloping = std::nullopt,
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
      std::optional<std::int64_t> galloping = std::nullopt,
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

  std::int64_t binary_index_after_timestamp(
      std::size_t begin,
      std::size_t end,
      std::uint64_t timestamp) const {
    const std::size_t index = lower_bound_timestamp(begin, end, timestamp);
    if (index >= size_) {
      return -1;
    }
    return static_cast<std::int64_t>(index);
  }

  std::size_t normalize_galloping_start(std::int64_t start) const {
    if (size_ == 0 || start <= 0) {
      return 0;
    }
    const auto unsigned_start = static_cast<std::uint64_t>(start);
    if (unsigned_start >= size_) {
      return size_ - 1;
    }
    return static_cast<std::size_t>(unsigned_start);
  }

  std::size_t capped_galloping_upper(
      std::size_t start,
      std::size_t step) const {
    if (step >= size_ - start) {
      return size_;
    }
    return start + step;
  }

  static std::size_t next_galloping_step(std::size_t step) {
    const std::size_t maximum_step = std::numeric_limits<std::size_t>::max() / 2;
    if (step > maximum_step) {
      return std::numeric_limits<std::size_t>::max();
    }
    return step * 2;
  }

  std::int64_t galloping_index_before_timestamp(
      std::uint64_t timestamp,
      std::int64_t start) const {
    const std::size_t start_index = normalize_galloping_start(start);

    if (timestamp_at(start_index) <= timestamp) {
      std::size_t lower = start_index;
      std::size_t step = 1;
      std::size_t upper = capped_galloping_upper(start_index, step);
      while (upper < size_ && timestamp_at(upper) <= timestamp) {
        lower = upper;
        step = next_galloping_step(step);
        upper = capped_galloping_upper(start_index, step);
      }

      return binary_index_before_timestamp(lower, std::min(upper, size_), timestamp);
    }

    std::size_t lower = start_index;
    std::size_t upper = start_index + 1;
    std::size_t step = 1;
    while (lower > 0 && timestamp_at(lower) > timestamp) {
      upper = lower;
      lower = (step >= start_index) ? 0 : start_index - step;
      step = next_galloping_step(step);
    }

    return binary_index_before_timestamp(lower, upper, timestamp);
  }

  std::int64_t galloping_index_after_timestamp(
      std::uint64_t timestamp,
      std::int64_t start) const {
    const std::size_t start_index = normalize_galloping_start(start);

    if (timestamp_at(start_index) >= timestamp) {
      std::size_t lower = start_index;
      std::size_t upper = start_index + 1;
    std::size_t step = 1;
    while (lower > 0 && timestamp_at(lower) >= timestamp) {
      upper = lower + 1;
      lower = (step >= start_index) ? 0 : start_index - step;
      step = next_galloping_step(step);
    }

      return binary_index_after_timestamp(lower, upper, timestamp);
    }

    std::size_t lower = start_index;
    std::size_t step = 1;
    std::size_t upper = capped_galloping_upper(start_index, step);
    while (upper < size_ && timestamp_at(upper) < timestamp) {
      lower = upper;
      step = next_galloping_step(step);
      upper = capped_galloping_upper(start_index, step);
    }

    return binary_index_after_timestamp(lower, std::min(upper + 1, size_), timestamp);
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
      std::optional<std::int64_t> galloping) const {
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
      std::optional<std::int64_t> galloping,
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
  std::string record_type_;
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

class CryptoTradeDatabase
    : public DatabaseRecordFile<CryptoTradeDatabaseTraits> {
 public:
  using DatabaseRecordFile::DatabaseRecordFile;
};

class CurrencyQuoteDatabase
    : public DatabaseRecordFile<CurrencyQuoteDatabaseTraits> {
 public:
  using DatabaseRecordFile::DatabaseRecordFile;
};

template <typename Traits>
class OptionDatabaseRecordFile : public DatabaseRecordFile<Traits> {
 public:
  using Base = DatabaseRecordFile<Traits>;
  using RowType = typename Traits::row_type;

  OptionDatabaseRecordFile(
      std::filesystem::path database_path,
      std::string date,
      std::string root,
      std::string expiration,
      std::string right,
      double strike)
      : Base(
            std::move(database_path),
            std::move(date),
            detail::option_contract_key(
                root,
                expiration,
                right,
                strike),
            std::string(Traits::record_type)),
        root_(std::move(root)),
        expiration_(std::move(expiration)),
        right_(std::move(right)),
        strike_millis_(detail::strike_millis_from_double(strike)),
        strike_(static_cast<double>(strike_millis_) / 1000.0) {}

  const std::string& root() const { return root_; }
  const std::string& expiration() const { return expiration_; }
  const std::string& right() const { return right_; }
  double strike() const { return strike_; }
  std::uint32_t strike_millis() const { return strike_millis_; }
  const std::string& contract_key() const { return Base::ticker(); }

 private:
  std::string root_;
  std::string expiration_;
  std::string right_;
  std::uint32_t strike_millis_ = 0;
  double strike_ = 0.0;
};

class OptionTradeDatabase
    : public OptionDatabaseRecordFile<OptionTradeDatabaseTraits> {
 public:
  using OptionDatabaseRecordFile::OptionDatabaseRecordFile;
};

class OptionQuoteDatabase
    : public OptionDatabaseRecordFile<OptionQuoteDatabaseTraits> {
 public:
  using OptionDatabaseRecordFile::OptionDatabaseRecordFile;
};

inline bool is_futures_exchange(std::string_view exchange) {
  return exchange == "cbot" ||
         exchange == "cme" ||
         exchange == "comex" ||
         exchange == "nymex";
}

inline std::string futures_database_record_type(
    std::string_view exchange,
    std::string_view kind) {
  if (kind != "trade" && kind != "quote") {
    throw std::invalid_argument("futures database kind must be trade or quote");
  }
  if (exchange.empty()) {
    std::string record_type = "future_";
    record_type += kind;
    return record_type;
  }
  if (!is_futures_exchange(exchange)) {
    std::ostringstream message;
    message << "unknown futures exchange '" << exchange
            << "'; expected one of cbot, cme, comex, nymex";
    throw std::invalid_argument(message.str());
  }

  std::string record_type = "future_";
  record_type += exchange;
  record_type += '_';
  record_type += kind;
  return record_type;
}

class FuturesTradeDatabase
    : public DatabaseRecordFile<FuturesTradeDatabaseTraits> {
 public:
  FuturesTradeDatabase(
      std::filesystem::path database_path,
      std::string date,
      std::string ticker,
      std::string exchange = {})
      : DatabaseRecordFile(
            std::move(database_path),
            std::move(date),
            std::move(ticker),
            futures_database_record_type(exchange, "trade")) {}
};

class FuturesQuoteDatabase
    : public DatabaseRecordFile<FuturesQuoteDatabaseTraits> {
 public:
  FuturesQuoteDatabase(
      std::filesystem::path database_path,
      std::string date,
      std::string ticker,
      std::string exchange = {})
      : DatabaseRecordFile(
            std::move(database_path),
            std::move(date),
            std::move(ticker),
            futures_database_record_type(exchange, "quote")) {}
};

class StockTradeQuoteTimeline {
 public:
  StockTradeQuoteTimeline(
      std::filesystem::path database_path,
      std::string date,
      std::string ticker)
      : trades_(database_path, date, ticker),
        quotes_(std::move(database_path), std::move(date), std::move(ticker)) {}

  StockTradeQuoteTimeline& iter() { return *this; }

  nanobind::tuple next() {
    if (trade_index_ >= trades_.size() && quote_index_ >= quotes_.size()) {
      throw nanobind::stop_iteration();
    }

    const bool next_is_quote =
        quote_index_ < quotes_.size() &&
        (trade_index_ >= trades_.size() ||
         StockQuote::sip_timestamp_at(quotes_.packed_data_at(quote_index_)) <=
             StockTrade::sip_timestamp_at(trades_.packed_data_at(trade_index_)));

    nanobind::tuple result = nanobind::steal<nanobind::tuple>(PyTuple_New(2));
    if (!result.is_valid()) {
      throw nanobind::python_error();
    }

    if (next_is_quote) {
      last_quote_ = quotes_.get_item(static_cast<std::int64_t>(quote_index_++));
      Py_INCREF(Py_None);
      PyTuple_SET_ITEM(result.ptr(), 0, Py_None);
      PyTuple_SET_ITEM(result.ptr(), 1, nanobind::cast(*last_quote_).release().ptr());
      return result;
    }

    StockTrade trade = trades_.get_item(static_cast<std::int64_t>(trade_index_++));
    PyTuple_SET_ITEM(result.ptr(), 0, nanobind::cast(std::move(trade)).release().ptr());
    if (last_quote_) {
      PyTuple_SET_ITEM(result.ptr(), 1, nanobind::cast(*last_quote_).release().ptr());
    } else {
      Py_INCREF(Py_None);
      PyTuple_SET_ITEM(result.ptr(), 1, Py_None);
    }
    return result;
  }

 private:
  StockTradeDatabase trades_;
  StockQuoteDatabase quotes_;
  std::size_t trade_index_ = 0;
  std::size_t quote_index_ = 0;
  std::optional<StockQuote> last_quote_;
};

struct SimpleMarketState {
  enum class EventKind : std::uint8_t {
    Quote = 0,
    Trade = 1,
  };

  struct Event {
    std::uint64_t timestamp = 0;
    std::size_t symbol_index = 0;
    EventKind kind = EventKind::Trade;
  };

  struct EventGreater {
    bool operator()(const Event& left, const Event& right) const {
      if (left.timestamp != right.timestamp) {
        return left.timestamp > right.timestamp;
      }
      if (left.symbol_index != right.symbol_index) {
        return left.symbol_index > right.symbol_index;
      }
      return static_cast<std::uint8_t>(left.kind) >
             static_cast<std::uint8_t>(right.kind);
    }
  };

  struct SymbolState {
    std::string symbol;
    nanobind::object symbol_object;
    std::unique_ptr<StockTradeDatabase> trades;
    std::unique_ptr<StockQuoteDatabase> quotes;
    std::size_t trade_index = 0;
    std::size_t quote_index = 0;
    std::int64_t last_quote_index = -1;
    std::int64_t last_execution_quote_index = -1;
    std::optional<StockTrade> last_trade;
    std::optional<StockQuote> last_quote;
  };

  SimpleMarketState(
      std::filesystem::path database_path,
      std::string date,
      const std::vector<std::string>& symbols,
      std::uint64_t trade_latency_ns,
      bool emit_quotes,
      bool fast)
      : database_path(std::move(database_path)),
        date(std::move(date)),
        trade_latency_ns(trade_latency_ns),
        emit_quotes(emit_quotes),
        fast(fast) {
    symbol_states.reserve(symbols.size());
    for (const auto& symbol : symbols) {
      SymbolState state;
      state.symbol = symbol;
      state.symbol_object = intern_symbol(symbol);
      state.trades = std::make_unique<StockTradeDatabase>(
          this->database_path,
          this->date,
          symbol);
      state.quotes = std::make_unique<StockQuoteDatabase>(
          this->database_path,
          this->date,
          symbol);

      const std::size_t symbol_index = symbol_states.size();
      if (state.trades->size() > 0) {
        event_queue.push(Event{
            StockTrade::sip_timestamp_at(state.trades->packed_data_at(0)),
            symbol_index,
            EventKind::Trade});
      }
      if (state.quotes->size() > 0) {
        event_queue.push(Event{
            StockQuote::sip_timestamp_at(state.quotes->packed_data_at(0)),
            symbol_index,
            EventKind::Quote});
      }

      holdings.emplace(symbol, 0.0);
      symbol_states.push_back(std::move(state));
    }
  }

  static nanobind::object intern_symbol(const std::string& symbol) {
    PyObject* object = PyUnicode_InternFromString(symbol.c_str());
    if (object == nullptr) {
      throw nanobind::python_error();
    }
    return nanobind::steal<nanobind::object>(object);
  }

  static std::uint64_t add_latency(
      std::uint64_t timestamp,
      std::uint64_t latency) {
    const std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
    if (maximum - timestamp < latency) {
      return maximum;
    }
    return timestamp + latency;
  }

  std::filesystem::path database_path;
  std::string date;
  std::uint64_t trade_latency_ns = 0;
  bool emit_quotes = false;
  bool fast = false;
  nanobind::dict last_trades_by_symbol;
  nanobind::dict last_quotes_by_symbol;
  std::vector<SymbolState> symbol_states;
  std::priority_queue<Event, std::vector<Event>, EventGreater> event_queue;
  std::unordered_map<std::string, double> holdings;
  double cash = 0.0;
};

class SimpleMarketBroker {
 public:
  SimpleMarketBroker(
      std::shared_ptr<SimpleMarketState> state,
      std::size_t symbol_index,
      std::uint64_t sip_timestamp)
      : state_(std::move(state)),
        symbol_index_(symbol_index),
        sip_timestamp_(sip_timestamp) {}

  nanobind::object symbol() const {
    return state_->symbol_states.at(symbol_index_).symbol_object;
  }

  std::uint64_t sip_timestamp() const { return sip_timestamp_; }

  void buy(double shares, std::optional<std::string> symbol = std::nullopt) {
    execute(shares, symbol, Side::Buy);
  }

  void sell(double shares, std::optional<std::string> symbol = std::nullopt) {
    execute(shares, symbol, Side::Sell);
  }

 private:
  enum class Side {
    Buy,
    Sell,
  };

  std::size_t resolve_symbol_index(const std::optional<std::string>& symbol) const {
    if (!symbol) {
      return symbol_index_;
    }
    for (std::size_t index = 0; index < state_->symbol_states.size(); ++index) {
      if (state_->symbol_states[index].symbol == *symbol) {
        return index;
      }
    }
    throw std::out_of_range("SimpleMarket broker received an unknown symbol");
  }

  void execute(
      double shares,
      const std::optional<std::string>& symbol,
      Side side) {
    if (!std::isfinite(shares) || shares < 0.0) {
      throw std::invalid_argument("shares must be a finite non-negative number");
    }

    const std::size_t target_symbol_index = resolve_symbol_index(symbol);
    auto& symbol_state = state_->symbol_states[target_symbol_index];
    const std::uint64_t target_timestamp =
        SimpleMarketState::add_latency(sip_timestamp_, state_->trade_latency_ns);
    const std::optional<std::int64_t> galloping =
        symbol_state.last_execution_quote_index >= 0
            ? std::optional<std::int64_t>(symbol_state.last_execution_quote_index)
            : std::optional<std::int64_t>(
                  symbol_state.last_quote_index >= 0 ? symbol_state.last_quote_index : 0);
    const std::int64_t quote_index =
        symbol_state.quotes->index_before_timestamp(target_timestamp, galloping);
    if (quote_index < 0) {
      throw std::out_of_range("no quote available at execution timestamp");
    }
    symbol_state.last_execution_quote_index = quote_index;

    const void* quote_data =
        symbol_state.quotes->packed_data_at(static_cast<std::size_t>(quote_index));
    const double price = side == Side::Buy
        ? StockQuote::ask_price_at(quote_data)
        : StockQuote::bid_price_at(quote_data);
    if (!std::isfinite(price) || price <= 0.0) {
      throw std::out_of_range("execution quote has no usable price");
    }

    double& position = state_->holdings[symbol_state.symbol];
    if (side == Side::Buy) {
      position += shares;
      state_->cash -= shares * price;
    } else {
      position -= shares;
      state_->cash += shares * price;
    }
  }

  std::shared_ptr<SimpleMarketState> state_;
  std::size_t symbol_index_ = 0;
  std::uint64_t sip_timestamp_ = 0;
};

class SimpleMarket {
 public:
  SimpleMarket(
      std::filesystem::path database_path,
      std::string date,
      const std::vector<std::string>& symbols,
      std::uint64_t trade_latency_ns,
      bool quotes,
      bool fast)
      : state_(std::make_shared<SimpleMarketState>(
            std::move(database_path),
            std::move(date),
            symbols,
            trade_latency_ns,
            quotes,
            fast)) {}

  SimpleMarket& iter() { return *this; }

  nanobind::tuple next() {
    while (!state_->event_queue.empty()) {
      const SimpleMarketState::Event event = state_->event_queue.top();
      state_->event_queue.pop();
      auto& symbol_state = state_->symbol_states[event.symbol_index];

      if (event.kind == SimpleMarketState::EventKind::Quote) {
        if (symbol_state.quote_index >= symbol_state.quotes->size()) {
          continue;
        }
        const auto quote_index = symbol_state.quote_index++;
        const void* quote_data = symbol_state.quotes->packed_data_at(quote_index);
        const std::uint64_t timestamp = StockQuote::sip_timestamp_at(quote_data);
        symbol_state.last_quote_index = static_cast<std::int64_t>(quote_index);
        symbol_state.last_quote = symbol_state.quotes->get_item(
            static_cast<std::int64_t>(quote_index));
        state_->last_quotes_by_symbol[symbol_state.symbol_object] =
            nanobind::cast(*symbol_state.last_quote);
        push_next_quote(event.symbol_index);
        if (!state_->emit_quotes) {
          continue;
        }
        current_broker_ = SimpleMarketBroker(state_, event.symbol_index, timestamp);
        return make_event_tuple(
            event.symbol_index,
            timestamp,
            std::nullopt,
            symbol_state.last_quote);
      }

      if (symbol_state.trade_index >= symbol_state.trades->size()) {
        continue;
      }
      const auto trade_index = symbol_state.trade_index++;
      const void* trade_data = symbol_state.trades->packed_data_at(trade_index);
      const std::uint64_t timestamp = StockTrade::sip_timestamp_at(trade_data);
      symbol_state.last_trade = symbol_state.trades->get_item(
          static_cast<std::int64_t>(trade_index));
      state_->last_trades_by_symbol[symbol_state.symbol_object] =
          nanobind::cast(*symbol_state.last_trade);
      push_next_trade(event.symbol_index);
      current_broker_ = SimpleMarketBroker(state_, event.symbol_index, timestamp);
      return make_event_tuple(
          event.symbol_index,
          timestamp,
          symbol_state.last_trade,
          std::nullopt);
    }

    throw nanobind::stop_iteration();
  }

  double get_holding(nanobind::handle key) const {
    if (key.is_none()) {
      return state_->cash;
    }
    const std::string symbol = nanobind::cast<std::string>(key);
    const auto iter = state_->holdings.find(symbol);
    if (iter == state_->holdings.end()) {
      throw std::out_of_range("SimpleMarket holdings key not found");
    }
    return iter->second;
  }

  bool contains(nanobind::handle key) const {
    if (key.is_none()) {
      return true;
    }
    if (!PyUnicode_Check(key.ptr())) {
      return false;
    }
    const std::string symbol = nanobind::cast<std::string>(key);
    return state_->holdings.find(symbol) != state_->holdings.end();
  }

  std::size_t size() const {
    return state_->holdings.size() + 1;
  }

  nanobind::list keys() const {
    nanobind::list result;
    Py_INCREF(Py_None);
    result.append(nanobind::borrow<nanobind::object>(Py_None));
    for (const auto& symbol_state : state_->symbol_states) {
      result.append(symbol_state.symbol_object);
    }
    return result;
  }

  nanobind::list values() const {
    nanobind::list result;
    result.append(state_->cash);
    for (const auto& symbol_state : state_->symbol_states) {
      const auto iter = state_->holdings.find(symbol_state.symbol);
      result.append(iter == state_->holdings.end() ? 0.0 : iter->second);
    }
    return result;
  }

  nanobind::list items() const {
    nanobind::list result;
    result.append(nanobind::make_tuple(nanobind::none(), state_->cash));
    for (const auto& symbol_state : state_->symbol_states) {
      const auto iter = state_->holdings.find(symbol_state.symbol);
      result.append(nanobind::make_tuple(
          symbol_state.symbol_object,
          iter == state_->holdings.end() ? 0.0 : iter->second));
    }
    return result;
  }

  nanobind::dict as_dict() const {
    nanobind::dict result;
    result[nanobind::none()] = state_->cash;
    for (const auto& symbol_state : state_->symbol_states) {
      const auto iter = state_->holdings.find(symbol_state.symbol);
      result[symbol_state.symbol_object] =
          iter == state_->holdings.end() ? 0.0 : iter->second;
    }
    return result;
  }

  SimpleMarketBroker broker() const {
    if (!current_broker_) {
      throw std::out_of_range("SimpleMarket broker is not available before iteration");
    }
    return *current_broker_;
  }

 private:
  void push_next_trade(std::size_t symbol_index) {
    auto& symbol_state = state_->symbol_states[symbol_index];
    if (symbol_state.trade_index < symbol_state.trades->size()) {
      state_->event_queue.push(SimpleMarketState::Event{
          StockTrade::sip_timestamp_at(
              symbol_state.trades->packed_data_at(symbol_state.trade_index)),
          symbol_index,
          SimpleMarketState::EventKind::Trade});
    }
  }

  void push_next_quote(std::size_t symbol_index) {
    auto& symbol_state = state_->symbol_states[symbol_index];
    if (symbol_state.quote_index < symbol_state.quotes->size()) {
      state_->event_queue.push(SimpleMarketState::Event{
          StockQuote::sip_timestamp_at(
              symbol_state.quotes->packed_data_at(symbol_state.quote_index)),
          symbol_index,
          SimpleMarketState::EventKind::Quote});
    }
  }

  nanobind::tuple make_event_tuple(
      std::size_t symbol_index,
      std::uint64_t timestamp_ns,
      const std::optional<StockTrade>& trade,
      const std::optional<StockQuote>& quote) {
    nanobind::tuple result = nanobind::steal<nanobind::tuple>(PyTuple_New(7));
    if (!result.is_valid()) {
      throw nanobind::python_error();
    }

    auto& symbol_state = state_->symbol_states[symbol_index];
    PyTuple_SET_ITEM(
        result.ptr(),
        0,
        nanobind::object(symbol_state.symbol_object).release().ptr());

    PyTuple_SET_ITEM(
        result.ptr(),
        1,
        nanobind::float_(static_cast<double>(timestamp_ns) / 1'000'000'000.0)
            .release()
            .ptr());

    if (trade) {
      PyTuple_SET_ITEM(result.ptr(), 2, nanobind::cast(*trade).release().ptr());
    } else {
      Py_INCREF(Py_None);
      PyTuple_SET_ITEM(result.ptr(), 2, Py_None);
    }

    if (quote) {
      PyTuple_SET_ITEM(result.ptr(), 3, nanobind::cast(*quote).release().ptr());
    } else {
      Py_INCREF(Py_None);
      PyTuple_SET_ITEM(result.ptr(), 3, Py_None);
    }

    PyTuple_SET_ITEM(
        result.ptr(),
        4,
        event_dict_object(state_->last_trades_by_symbol).release().ptr());
    PyTuple_SET_ITEM(
        result.ptr(),
        5,
        event_dict_object(state_->last_quotes_by_symbol).release().ptr());
    PyTuple_SET_ITEM(
        result.ptr(),
        6,
        nanobind::cast(*current_broker_).release().ptr());
    return result;
  }

  nanobind::object event_dict_object(const nanobind::dict& source) const {
    if (state_->fast) {
      return nanobind::object(source);
    }
    PyObject* copy = PyDict_Copy(source.ptr());
    if (copy == nullptr) {
      throw nanobind::python_error();
    }
    return nanobind::steal<nanobind::object>(copy);
  }

  std::shared_ptr<SimpleMarketState> state_;
  std::optional<SimpleMarketBroker> current_broker_;
};

struct FuturesMarketState {
  enum class EventKind : std::uint8_t {
    Quote = 0,
    Trade = 1,
  };

  struct Event {
    std::uint64_t timestamp = 0;
    std::size_t symbol_index = 0;
    EventKind kind = EventKind::Trade;
  };

  struct EventGreater {
    bool operator()(const Event& left, const Event& right) const {
      if (left.timestamp != right.timestamp) {
        return left.timestamp > right.timestamp;
      }
      if (left.symbol_index != right.symbol_index) {
        return left.symbol_index > right.symbol_index;
      }
      return static_cast<std::uint8_t>(left.kind) >
             static_cast<std::uint8_t>(right.kind);
    }
  };

  struct SymbolState {
    std::string symbol;
    nanobind::object symbol_object;
    std::unique_ptr<FuturesTradeDatabase> trades;
    std::unique_ptr<FuturesQuoteDatabase> quotes;
    std::size_t trade_index = 0;
    std::size_t quote_index = 0;
    std::int64_t last_quote_index = -1;
    std::int64_t last_execution_quote_index = -1;
    std::optional<FuturesTrade> last_trade;
    std::optional<FuturesQuote> last_quote;
  };

  FuturesMarketState(
      std::filesystem::path database_path,
      std::string date,
      std::string exchange,
      const std::vector<std::string>& symbols,
      std::uint64_t trade_latency_ns,
      bool emit_quotes,
      bool fast)
      : database_path(std::move(database_path)),
        date(std::move(date)),
        exchange(std::move(exchange)),
        trade_latency_ns(trade_latency_ns),
        emit_quotes(emit_quotes),
        fast(fast) {
    symbol_states.reserve(symbols.size());
    for (const auto& symbol : symbols) {
      SymbolState state;
      state.symbol = symbol;
      state.symbol_object = intern_symbol(symbol);
      state.trades = std::make_unique<FuturesTradeDatabase>(
          this->database_path,
          this->date,
          symbol,
          this->exchange);
      state.quotes = std::make_unique<FuturesQuoteDatabase>(
          this->database_path,
          this->date,
          symbol,
          this->exchange);

      const std::size_t symbol_index = symbol_states.size();
      if (state.trades->size() > 0) {
        event_queue.push(Event{
            FuturesTrade::timestamp_at(state.trades->packed_data_at(0)),
            symbol_index,
            EventKind::Trade});
      }
      if (state.quotes->size() > 0) {
        event_queue.push(Event{
            FuturesQuote::timestamp_at(state.quotes->packed_data_at(0)),
            symbol_index,
            EventKind::Quote});
      }

      holdings.emplace(symbol, 0.0);
      symbol_states.push_back(std::move(state));
    }
  }

  static nanobind::object intern_symbol(const std::string& symbol) {
    PyObject* object = PyUnicode_InternFromString(symbol.c_str());
    if (object == nullptr) {
      throw nanobind::python_error();
    }
    return nanobind::steal<nanobind::object>(object);
  }

  static std::uint64_t add_latency(
      std::uint64_t timestamp,
      std::uint64_t latency) {
    const std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
    if (maximum - timestamp < latency) {
      return maximum;
    }
    return timestamp + latency;
  }

  std::filesystem::path database_path;
  std::string date;
  std::string exchange;
  std::uint64_t trade_latency_ns = 0;
  bool emit_quotes = false;
  bool fast = false;
  nanobind::dict last_trades_by_symbol;
  nanobind::dict last_quotes_by_symbol;
  std::vector<SymbolState> symbol_states;
  std::priority_queue<Event, std::vector<Event>, EventGreater> event_queue;
  std::unordered_map<std::string, double> holdings;
  double cash = 0.0;
};

class FuturesMarketBroker {
 public:
  FuturesMarketBroker(
      std::shared_ptr<FuturesMarketState> state,
      std::size_t symbol_index,
      std::uint64_t timestamp)
      : state_(std::move(state)),
        symbol_index_(symbol_index),
        timestamp_(timestamp) {}

  nanobind::object symbol() const {
    return state_->symbol_states.at(symbol_index_).symbol_object;
  }

  std::uint64_t timestamp() const { return timestamp_; }
  std::uint64_t sip_timestamp() const { return timestamp_; }

  void buy(double contracts, std::optional<std::string> symbol = std::nullopt) {
    execute(contracts, symbol, Side::Buy);
  }

  void sell(double contracts, std::optional<std::string> symbol = std::nullopt) {
    execute(contracts, symbol, Side::Sell);
  }

 private:
  enum class Side {
    Buy,
    Sell,
  };

  std::size_t resolve_symbol_index(const std::optional<std::string>& symbol) const {
    if (!symbol) {
      return symbol_index_;
    }
    for (std::size_t index = 0; index < state_->symbol_states.size(); ++index) {
      if (state_->symbol_states[index].symbol == *symbol) {
        return index;
      }
    }
    throw std::out_of_range("FuturesMarket broker received an unknown symbol");
  }

  void execute(
      double contracts,
      const std::optional<std::string>& symbol,
      Side side) {
    if (!std::isfinite(contracts) || contracts < 0.0) {
      throw std::invalid_argument("contracts must be a finite non-negative number");
    }

    const std::size_t target_symbol_index = resolve_symbol_index(symbol);
    auto& symbol_state = state_->symbol_states[target_symbol_index];
    const std::uint64_t target_timestamp =
        FuturesMarketState::add_latency(timestamp_, state_->trade_latency_ns);
    const std::optional<std::int64_t> galloping =
        symbol_state.last_execution_quote_index >= 0
            ? std::optional<std::int64_t>(symbol_state.last_execution_quote_index)
            : std::optional<std::int64_t>(
                  symbol_state.last_quote_index >= 0 ? symbol_state.last_quote_index : 0);
    const std::int64_t quote_index =
        symbol_state.quotes->index_before_timestamp(target_timestamp, galloping);
    if (quote_index < 0) {
      throw std::out_of_range("no futures quote available at execution timestamp");
    }
    symbol_state.last_execution_quote_index = quote_index;

    const void* quote_data =
        symbol_state.quotes->packed_data_at(static_cast<std::size_t>(quote_index));
    const double price = side == Side::Buy
        ? FuturesQuote::ask_price_at(quote_data)
        : FuturesQuote::bid_price_at(quote_data);
    if (!std::isfinite(price) || price <= 0.0) {
      throw std::out_of_range("execution futures quote has no usable price");
    }

    double& position = state_->holdings[symbol_state.symbol];
    if (side == Side::Buy) {
      position += contracts;
      state_->cash -= contracts * price;
    } else {
      position -= contracts;
      state_->cash += contracts * price;
    }
  }

  std::shared_ptr<FuturesMarketState> state_;
  std::size_t symbol_index_ = 0;
  std::uint64_t timestamp_ = 0;
};

class FuturesMarket {
 public:
  FuturesMarket(
      std::filesystem::path database_path,
      std::string date,
      const std::vector<std::string>& symbols,
      std::uint64_t trade_latency_ns,
      std::string exchange,
      bool quotes,
      bool fast)
      : state_(std::make_shared<FuturesMarketState>(
            std::move(database_path),
            std::move(date),
            std::move(exchange),
            symbols,
            trade_latency_ns,
            quotes,
            fast)) {}

  FuturesMarket& iter() { return *this; }

  nanobind::tuple next() {
    while (!state_->event_queue.empty()) {
      const FuturesMarketState::Event event = state_->event_queue.top();
      state_->event_queue.pop();
      auto& symbol_state = state_->symbol_states[event.symbol_index];

      if (event.kind == FuturesMarketState::EventKind::Quote) {
        if (symbol_state.quote_index >= symbol_state.quotes->size()) {
          continue;
        }
        const auto quote_index = symbol_state.quote_index++;
        const void* quote_data = symbol_state.quotes->packed_data_at(quote_index);
        const std::uint64_t timestamp = FuturesQuote::timestamp_at(quote_data);
        symbol_state.last_quote_index = static_cast<std::int64_t>(quote_index);
        symbol_state.last_quote = symbol_state.quotes->get_item(
            static_cast<std::int64_t>(quote_index));
        state_->last_quotes_by_symbol[symbol_state.symbol_object] =
            nanobind::cast(*symbol_state.last_quote);
        push_next_quote(event.symbol_index);
        if (!state_->emit_quotes) {
          continue;
        }
        current_broker_ = FuturesMarketBroker(state_, event.symbol_index, timestamp);
        return make_event_tuple(
            event.symbol_index,
            timestamp,
            std::nullopt,
            symbol_state.last_quote);
      }

      if (symbol_state.trade_index >= symbol_state.trades->size()) {
        continue;
      }
      const auto trade_index = symbol_state.trade_index++;
      const void* trade_data = symbol_state.trades->packed_data_at(trade_index);
      const std::uint64_t timestamp = FuturesTrade::timestamp_at(trade_data);
      symbol_state.last_trade = symbol_state.trades->get_item(
          static_cast<std::int64_t>(trade_index));
      state_->last_trades_by_symbol[symbol_state.symbol_object] =
          nanobind::cast(*symbol_state.last_trade);
      push_next_trade(event.symbol_index);
      current_broker_ = FuturesMarketBroker(state_, event.symbol_index, timestamp);
      return make_event_tuple(
          event.symbol_index,
          timestamp,
          symbol_state.last_trade,
          std::nullopt);
    }

    throw nanobind::stop_iteration();
  }

  double get_holding(nanobind::handle key) const {
    if (key.is_none()) {
      return state_->cash;
    }
    const std::string symbol = nanobind::cast<std::string>(key);
    const auto iter = state_->holdings.find(symbol);
    if (iter == state_->holdings.end()) {
      throw std::out_of_range("FuturesMarket holdings key not found");
    }
    return iter->second;
  }

  bool contains(nanobind::handle key) const {
    if (key.is_none()) {
      return true;
    }
    if (!PyUnicode_Check(key.ptr())) {
      return false;
    }
    const std::string symbol = nanobind::cast<std::string>(key);
    return state_->holdings.find(symbol) != state_->holdings.end();
  }

  std::size_t size() const {
    return state_->holdings.size() + 1;
  }

  nanobind::list keys() const {
    nanobind::list result;
    Py_INCREF(Py_None);
    result.append(nanobind::borrow<nanobind::object>(Py_None));
    for (const auto& symbol_state : state_->symbol_states) {
      result.append(symbol_state.symbol_object);
    }
    return result;
  }

  nanobind::list values() const {
    nanobind::list result;
    result.append(state_->cash);
    for (const auto& symbol_state : state_->symbol_states) {
      const auto iter = state_->holdings.find(symbol_state.symbol);
      result.append(iter == state_->holdings.end() ? 0.0 : iter->second);
    }
    return result;
  }

  nanobind::list items() const {
    nanobind::list result;
    result.append(nanobind::make_tuple(nanobind::none(), state_->cash));
    for (const auto& symbol_state : state_->symbol_states) {
      const auto iter = state_->holdings.find(symbol_state.symbol);
      result.append(nanobind::make_tuple(
          symbol_state.symbol_object,
          iter == state_->holdings.end() ? 0.0 : iter->second));
    }
    return result;
  }

  nanobind::dict as_dict() const {
    nanobind::dict result;
    result[nanobind::none()] = state_->cash;
    for (const auto& symbol_state : state_->symbol_states) {
      const auto iter = state_->holdings.find(symbol_state.symbol);
      result[symbol_state.symbol_object] =
          iter == state_->holdings.end() ? 0.0 : iter->second;
    }
    return result;
  }

  FuturesMarketBroker broker() const {
    if (!current_broker_) {
      throw std::out_of_range("FuturesMarket broker is not available before iteration");
    }
    return *current_broker_;
  }

 private:
  void push_next_trade(std::size_t symbol_index) {
    auto& symbol_state = state_->symbol_states[symbol_index];
    if (symbol_state.trade_index < symbol_state.trades->size()) {
      state_->event_queue.push(FuturesMarketState::Event{
          FuturesTrade::timestamp_at(
              symbol_state.trades->packed_data_at(symbol_state.trade_index)),
          symbol_index,
          FuturesMarketState::EventKind::Trade});
    }
  }

  void push_next_quote(std::size_t symbol_index) {
    auto& symbol_state = state_->symbol_states[symbol_index];
    if (symbol_state.quote_index < symbol_state.quotes->size()) {
      state_->event_queue.push(FuturesMarketState::Event{
          FuturesQuote::timestamp_at(
              symbol_state.quotes->packed_data_at(symbol_state.quote_index)),
          symbol_index,
          FuturesMarketState::EventKind::Quote});
    }
  }

  nanobind::tuple make_event_tuple(
      std::size_t symbol_index,
      std::uint64_t timestamp_ns,
      const std::optional<FuturesTrade>& trade,
      const std::optional<FuturesQuote>& quote) {
    nanobind::tuple result = nanobind::steal<nanobind::tuple>(PyTuple_New(7));
    if (!result.is_valid()) {
      throw nanobind::python_error();
    }

    auto& symbol_state = state_->symbol_states[symbol_index];
    PyTuple_SET_ITEM(
        result.ptr(),
        0,
        nanobind::object(symbol_state.symbol_object).release().ptr());

    PyTuple_SET_ITEM(
        result.ptr(),
        1,
        nanobind::float_(static_cast<double>(timestamp_ns) / 1'000'000'000.0)
            .release()
            .ptr());

    if (trade) {
      PyTuple_SET_ITEM(result.ptr(), 2, nanobind::cast(*trade).release().ptr());
    } else {
      Py_INCREF(Py_None);
      PyTuple_SET_ITEM(result.ptr(), 2, Py_None);
    }

    if (quote) {
      PyTuple_SET_ITEM(result.ptr(), 3, nanobind::cast(*quote).release().ptr());
    } else {
      Py_INCREF(Py_None);
      PyTuple_SET_ITEM(result.ptr(), 3, Py_None);
    }

    PyTuple_SET_ITEM(
        result.ptr(),
        4,
        event_dict_object(state_->last_trades_by_symbol).release().ptr());
    PyTuple_SET_ITEM(
        result.ptr(),
        5,
        event_dict_object(state_->last_quotes_by_symbol).release().ptr());
    PyTuple_SET_ITEM(
        result.ptr(),
        6,
        nanobind::cast(*current_broker_).release().ptr());
    return result;
  }

  nanobind::object event_dict_object(const nanobind::dict& source) const {
    if (state_->fast) {
      return nanobind::object(source);
    }
    PyObject* copy = PyDict_Copy(source.ptr());
    if (copy == nullptr) {
      throw nanobind::python_error();
    }
    return nanobind::steal<nanobind::object>(copy);
  }

  std::shared_ptr<FuturesMarketState> state_;
  std::optional<FuturesMarketBroker> current_broker_;
};

struct OptionMarketState {
  enum class EventKind : std::uint8_t {
    Quote = 0,
    Trade = 1,
  };

  OptionMarketState(
      std::filesystem::path database_path,
      std::string date,
      std::string root,
      std::string expiration,
      std::string right,
      double strike,
      std::uint64_t trade_latency_ns,
      bool emit_quotes,
      bool fast)
      : database_path(std::move(database_path)),
        date(std::move(date)),
        root(std::move(root)),
        expiration(std::move(expiration)),
        right(std::move(right)),
        strike(strike),
        strike_millis(detail::strike_millis_from_double(strike)),
        contract_key(detail::option_contract_key(
            this->root,
            this->expiration,
            this->right,
            this->strike)),
        contract_object(nanobind::make_tuple(
            this->root,
            this->expiration,
            this->right,
            static_cast<double>(this->strike_millis) / 1000.0)),
        trades(std::make_unique<OptionTradeDatabase>(
            this->database_path,
            this->date,
            this->root,
            this->expiration,
            this->right,
            this->strike)),
        quotes(std::make_unique<OptionQuoteDatabase>(
            this->database_path,
            this->date,
            this->root,
            this->expiration,
            this->right,
            this->strike)),
        trade_latency_ns(trade_latency_ns),
        emit_quotes(emit_quotes),
        fast(fast) {}

  static std::uint64_t add_latency(
      std::uint64_t timestamp,
      std::uint64_t latency) {
    const std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
    if (maximum - timestamp < latency) {
      return maximum;
    }
    return timestamp + latency;
  }

  std::filesystem::path database_path;
  std::string date;
  std::string root;
  std::string expiration;
  std::string right;
  double strike = 0.0;
  std::uint32_t strike_millis = 0;
  std::string contract_key;
  nanobind::object contract_object;
  std::unique_ptr<OptionTradeDatabase> trades;
  std::unique_ptr<OptionQuoteDatabase> quotes;
  std::size_t trade_index = 0;
  std::size_t quote_index = 0;
  std::int64_t last_quote_index = -1;
  std::int64_t last_execution_quote_index = -1;
  std::optional<OptionTrade> last_trade;
  std::optional<OptionQuote> last_quote;
  std::uint64_t trade_latency_ns = 0;
  bool emit_quotes = false;
  bool fast = false;
  nanobind::dict last_trades_by_contract;
  nanobind::dict last_quotes_by_contract;
  double position = 0.0;
  double cash = 0.0;
};

class OptionMarketBroker {
 public:
  OptionMarketBroker(
      std::shared_ptr<OptionMarketState> state,
      std::uint64_t sip_timestamp)
      : state_(std::move(state)),
        sip_timestamp_(sip_timestamp) {}

  nanobind::object contract() const {
    return nanobind::object(state_->contract_object);
  }

  std::uint64_t sip_timestamp() const { return sip_timestamp_; }

  void buy(double contracts) {
    execute(contracts, Side::Buy);
  }

  void sell(double contracts) {
    execute(contracts, Side::Sell);
  }

 private:
  enum class Side {
    Buy,
    Sell,
  };

  void execute(double contracts, Side side) {
    if (!std::isfinite(contracts) || contracts < 0.0) {
      throw std::invalid_argument("contracts must be a finite non-negative number");
    }

    const std::uint64_t target_timestamp =
        OptionMarketState::add_latency(sip_timestamp_, state_->trade_latency_ns);
    const std::optional<std::int64_t> galloping =
        state_->last_execution_quote_index >= 0
            ? std::optional<std::int64_t>(state_->last_execution_quote_index)
            : std::optional<std::int64_t>(
                  state_->last_quote_index >= 0 ? state_->last_quote_index : 0);
    const std::int64_t quote_index =
        state_->quotes->index_before_timestamp(target_timestamp, galloping);
    if (quote_index < 0) {
      throw std::out_of_range("no option quote available at execution timestamp");
    }
    state_->last_execution_quote_index = quote_index;

    const void* quote_data =
        state_->quotes->packed_data_at(static_cast<std::size_t>(quote_index));
    const double price = side == Side::Buy
        ? OptionQuote::ask_price_at(quote_data)
        : OptionQuote::bid_price_at(quote_data);
    if (!std::isfinite(price) || price <= 0.0) {
      throw std::out_of_range("execution option quote has no usable price");
    }

    if (side == Side::Buy) {
      state_->position += contracts;
      state_->cash -= contracts * price;
    } else {
      state_->position -= contracts;
      state_->cash += contracts * price;
    }
  }

  std::shared_ptr<OptionMarketState> state_;
  std::uint64_t sip_timestamp_ = 0;
};

class OptionMarket {
 public:
  OptionMarket(
      std::filesystem::path database_path,
      std::string date,
      std::string root,
      std::string expiration,
      std::string right,
      double strike,
      std::uint64_t trade_latency_ns,
      bool quotes,
      bool fast)
      : state_(std::make_shared<OptionMarketState>(
            std::move(database_path),
            std::move(date),
            std::move(root),
            std::move(expiration),
            std::move(right),
            strike,
            trade_latency_ns,
            quotes,
            fast)) {}

  OptionMarket& iter() { return *this; }

  nanobind::tuple next() {
    while (state_->trade_index < state_->trades->size() ||
           state_->quote_index < state_->quotes->size()) {
      const bool next_is_quote =
          state_->quote_index < state_->quotes->size() &&
          (state_->trade_index >= state_->trades->size() ||
           OptionQuote::sip_timestamp_at(state_->quotes->packed_data_at(state_->quote_index)) <=
               OptionTrade::sip_timestamp_at(state_->trades->packed_data_at(state_->trade_index)));

      if (next_is_quote) {
        const std::size_t quote_index = state_->quote_index++;
        const void* quote_data = state_->quotes->packed_data_at(quote_index);
        const std::uint64_t timestamp = OptionQuote::sip_timestamp_at(quote_data);
        state_->last_quote_index = static_cast<std::int64_t>(quote_index);
        state_->last_quote =
            state_->quotes->get_item(static_cast<std::int64_t>(quote_index));
        state_->last_quotes_by_contract[state_->contract_object] =
            nanobind::cast(*state_->last_quote);
        if (!state_->emit_quotes) {
          continue;
        }
        current_broker_ = OptionMarketBroker(state_, timestamp);
        return make_event_tuple(timestamp, std::nullopt, state_->last_quote);
      }

      const std::size_t trade_index = state_->trade_index++;
      const void* trade_data = state_->trades->packed_data_at(trade_index);
      const std::uint64_t timestamp = OptionTrade::sip_timestamp_at(trade_data);
      state_->last_trade =
          state_->trades->get_item(static_cast<std::int64_t>(trade_index));
      state_->last_trades_by_contract[state_->contract_object] =
          nanobind::cast(*state_->last_trade);
      current_broker_ = OptionMarketBroker(state_, timestamp);
      return make_event_tuple(timestamp, state_->last_trade, std::nullopt);
    }

    throw nanobind::stop_iteration();
  }

  double get_holding(nanobind::handle key) const {
    if (key.is_none()) {
      return state_->cash;
    }
    if (is_contract_key(key)) {
      return state_->position;
    }
    throw std::out_of_range("OptionMarket holdings key not found");
  }

  bool contains(nanobind::handle key) const {
    return key.is_none() || is_contract_key(key);
  }

  std::size_t size() const { return 2; }

  nanobind::list keys() const {
    nanobind::list result;
    Py_INCREF(Py_None);
    result.append(nanobind::borrow<nanobind::object>(Py_None));
    result.append(state_->contract_object);
    return result;
  }

  nanobind::list values() const {
    nanobind::list result;
    result.append(state_->cash);
    result.append(state_->position);
    return result;
  }

  nanobind::list items() const {
    nanobind::list result;
    result.append(nanobind::make_tuple(nanobind::none(), state_->cash));
    result.append(nanobind::make_tuple(state_->contract_object, state_->position));
    return result;
  }

  nanobind::dict as_dict() const {
    nanobind::dict result;
    result[nanobind::none()] = state_->cash;
    result[state_->contract_object] = state_->position;
    return result;
  }

  OptionMarketBroker broker() const {
    if (!current_broker_) {
      throw std::out_of_range("OptionMarket broker is not available before iteration");
    }
    return *current_broker_;
  }

 private:
  bool is_contract_key(nanobind::handle key) const {
    if (PyUnicode_Check(key.ptr())) {
      return nanobind::cast<std::string>(key) == state_->contract_key;
    }
    int equal = PyObject_RichCompareBool(key.ptr(), state_->contract_object.ptr(), Py_EQ);
    if (equal < 0) {
      throw nanobind::python_error();
    }
    return equal == 1;
  }

  nanobind::tuple make_event_tuple(
      std::uint64_t timestamp_ns,
      const std::optional<OptionTrade>& trade,
      const std::optional<OptionQuote>& quote) {
    nanobind::tuple result = nanobind::steal<nanobind::tuple>(PyTuple_New(7));
    if (!result.is_valid()) {
      throw nanobind::python_error();
    }

    PyTuple_SET_ITEM(
        result.ptr(),
        0,
        nanobind::object(state_->contract_object).release().ptr());

    PyTuple_SET_ITEM(
        result.ptr(),
        1,
        nanobind::float_(static_cast<double>(timestamp_ns) / 1'000'000'000.0)
            .release()
            .ptr());

    if (trade) {
      PyTuple_SET_ITEM(result.ptr(), 2, nanobind::cast(*trade).release().ptr());
    } else {
      Py_INCREF(Py_None);
      PyTuple_SET_ITEM(result.ptr(), 2, Py_None);
    }

    if (quote) {
      PyTuple_SET_ITEM(result.ptr(), 3, nanobind::cast(*quote).release().ptr());
    } else {
      Py_INCREF(Py_None);
      PyTuple_SET_ITEM(result.ptr(), 3, Py_None);
    }

    PyTuple_SET_ITEM(
        result.ptr(),
        4,
        event_dict_object(state_->last_trades_by_contract).release().ptr());
    PyTuple_SET_ITEM(
        result.ptr(),
        5,
        event_dict_object(state_->last_quotes_by_contract).release().ptr());
    PyTuple_SET_ITEM(
        result.ptr(),
        6,
        nanobind::cast(*current_broker_).release().ptr());
    return result;
  }

  nanobind::object event_dict_object(const nanobind::dict& source) const {
    if (state_->fast) {
      return nanobind::object(source);
    }
    PyObject* copy = PyDict_Copy(source.ptr());
    if (copy == nullptr) {
      throw nanobind::python_error();
    }
    return nanobind::steal<nanobind::object>(copy);
  }

  std::shared_ptr<OptionMarketState> state_;
  std::optional<OptionMarketBroker> current_broker_;
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

template <typename Specialization>
class SharedRawLineRowsIterator {
 public:
  explicit SharedRawLineRowsIterator(const std::filesystem::path& path)
      : reader_(path) {}

  SharedRawLineRowsIterator& iter() { return *this; }

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

template <typename Base, typename Specialization = NativeSpecialization>
class Implementation : public Base {
 public:
  using Base::Base;
  using specialization_type = Specialization;
  using GzipLineGenerator = std::generator<std::string>;
  using GzipLineIteratorType = decltype(std::declval<GzipLineGenerator&>().begin());
  using RawStockTrade = std::array<std::string, 13>;
  using RawCryptoTrade = std::array<std::string, 7>;
  using RawOptionTrade = std::array<std::string, 7>;
  using RawOptionQuote = std::array<std::string, 9>;
  using RawStockQuote = std::array<std::string, 14>;
  using RawStockAggregate = std::array<std::string, 8>;
  using RawCurrencyQuote = std::array<std::string, 6>;
  using RawCurrencyAggregate = std::array<std::string, 8>;
  using RawLineRowsIterator = SharedRawLineRowsIterator<Specialization>;

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

  class CryptoTradeStreamState {
   public:
    explicit CryptoTradeStreamState(const std::filesystem::path& path)
        : reader_(path) {}

    bool next_row(CryptoTrade& row) {
      std::string_view line;

      while (reader_.template next_line<Specialization>(line)) {
        if (is_first_line_) {
          is_first_line_ = false;
          continue;
        }

        if (line.empty()) {
          continue;
        }

        row = Implementation::parse_crypto_trade_row(line, bitset_cache_);
        return true;
      }

      return false;
    }

   private:
    detail::BufferedGzipLineReader reader_;
    bool is_first_line_ = true;
    detail::BitsetParseCache<96> bitset_cache_;
  };

  class OptionTradeStreamState {
   public:
    explicit OptionTradeStreamState(const std::filesystem::path& path)
        : reader_(path) {}

    bool next_row(OptionTrade& row) {
      while (true) {
        if (row_index_ < rows_.size()) {
          row = std::move(rows_[row_index_++]);
          return true;
        }

        if (!load_next_root()) {
          return false;
        }
      }
    }

   private:
    bool load_next_root() {
      rows_.clear();
      row_index_ = 0;

      OptionTrade first;
      if (pending_row_) {
        first = std::move(*pending_row_);
        pending_row_.reset();
      } else if (!read_next_parsed_row(first)) {
        return false;
      }

      const std::string current_root = first.root;
      rows_.push_back(std::move(first));

      OptionTrade candidate;
      while (read_next_parsed_row(candidate)) {
        if (candidate.root != current_root) {
          pending_row_ = std::move(candidate);
          break;
        }
        rows_.push_back(std::move(candidate));
      }

      std::stable_sort(
          rows_.begin(),
          rows_.end(),
          [](const OptionTrade& lhs, const OptionTrade& rhs) {
            if (lhs.sip_timestamp != rhs.sip_timestamp) {
              return lhs.sip_timestamp < rhs.sip_timestamp;
            }
            if (lhs.expiration != rhs.expiration) {
              return lhs.expiration < rhs.expiration;
            }
            if (lhs.right != rhs.right) {
              return lhs.right < rhs.right;
            }
            return lhs.strike_millis < rhs.strike_millis;
          });
      return true;
    }

    bool read_next_parsed_row(OptionTrade& row) {
      std::string_view line;

      while (reader_.template next_line<Specialization>(line)) {
        if (is_first_line_) {
          is_first_line_ = false;
          continue;
        }

        if (line.empty()) {
          continue;
        }

        row = Implementation::parse_option_trade_row(line);
        return true;
      }

      return false;
    }

    detail::BufferedGzipLineReader reader_;
    bool is_first_line_ = true;
    std::optional<OptionTrade> pending_row_;
    std::vector<OptionTrade> rows_;
    std::size_t row_index_ = 0;
  };

  class OptionQuoteStreamState {
   public:
    explicit OptionQuoteStreamState(const std::filesystem::path& path)
        : reader_(path) {}

    bool next_row(OptionQuote& row) {
      while (true) {
        if (row_index_ < rows_.size()) {
          row = std::move(rows_[row_index_++]);
          return true;
        }

        if (!load_next_root()) {
          return false;
        }
      }
    }

   private:
    bool load_next_root() {
      rows_.clear();
      row_index_ = 0;

      OptionQuote first;
      if (pending_row_) {
        first = std::move(*pending_row_);
        pending_row_.reset();
      } else if (!read_next_parsed_row(first)) {
        return false;
      }

      const std::string current_root = first.root;
      rows_.push_back(std::move(first));

      OptionQuote candidate;
      while (read_next_parsed_row(candidate)) {
        if (candidate.root != current_root) {
          pending_row_ = std::move(candidate);
          break;
        }
        rows_.push_back(std::move(candidate));
      }

      std::stable_sort(
          rows_.begin(),
          rows_.end(),
          [](const OptionQuote& lhs, const OptionQuote& rhs) {
            if (lhs.sip_timestamp != rhs.sip_timestamp) {
              return lhs.sip_timestamp < rhs.sip_timestamp;
            }
            if (lhs.expiration != rhs.expiration) {
              return lhs.expiration < rhs.expiration;
            }
            if (lhs.right != rhs.right) {
              return lhs.right < rhs.right;
            }
            if (lhs.strike_millis != rhs.strike_millis) {
              return lhs.strike_millis < rhs.strike_millis;
            }
            return lhs.sequence_number < rhs.sequence_number;
          });
      return true;
    }

    bool read_next_parsed_row(OptionQuote& row) {
      std::string_view line;

      while (reader_.template next_line<Specialization>(line)) {
        if (is_first_line_) {
          is_first_line_ = false;
          continue;
        }

        if (line.empty()) {
          continue;
        }

        row = Implementation::parse_option_quote_row(line);
        return true;
      }

      return false;
    }

    detail::BufferedGzipLineReader reader_;
    bool is_first_line_ = true;
    std::optional<OptionQuote> pending_row_;
    std::vector<OptionQuote> rows_;
    std::size_t row_index_ = 0;
  };

  class FuturesTradeStreamState {
   public:
    explicit FuturesTradeStreamState(const std::filesystem::path& path)
        : reader_(path) {}

    bool next_row(FuturesTrade& row) {
      std::string_view line;

      while (reader_.template next_line<Specialization>(line)) {
        if (is_first_line_) {
          is_first_line_ = false;
          continue;
        }

        if (line.empty()) {
          continue;
        }

        row = Implementation::parse_futures_trade_row(line);
        return true;
      }

      return false;
    }

   private:
    detail::BufferedGzipLineReader reader_;
    bool is_first_line_ = true;
  };

  class FuturesQuoteStreamState {
   public:
    explicit FuturesQuoteStreamState(const std::filesystem::path& path)
        : reader_(path) {}

    bool next_row(FuturesQuote& row) {
      std::string_view line;

      while (reader_.template next_line<Specialization>(line)) {
        if (is_first_line_) {
          is_first_line_ = false;
          continue;
        }

        if (line.empty()) {
          continue;
        }

        row = Implementation::parse_futures_quote_row(line);
        return true;
      }

      return false;
    }

   private:
    detail::BufferedGzipLineReader reader_;
    bool is_first_line_ = true;
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

  class RawCryptoTradeStreamState {
   public:
    explicit RawCryptoTradeStreamState(const std::filesystem::path& path)
        : reader_(path) {}

    RawCryptoTradeStreamState(const RawCryptoTradeStreamState&) = delete;
    RawCryptoTradeStreamState& operator=(const RawCryptoTradeStreamState&) = delete;

    RawCryptoTradeStreamState(RawCryptoTradeStreamState&& other) noexcept
        : reader_(std::move(other.reader_)),
          is_first_line_(other.is_first_line_),
          intern_cache_(std::move(other.intern_cache_)) {}

    RawCryptoTradeStreamState& operator=(RawCryptoTradeStreamState&& other) noexcept {
      if (this == &other) {
        return *this;
      }

      reader_ = std::move(other.reader_);
      is_first_line_ = other.is_first_line_;
      intern_cache_ = std::move(other.intern_cache_);
      return *this;
    }

    bool next_row(RawCryptoTrade& row) {
      std::string_view line;

      while (reader_.template next_line<Specialization>(line)) {
        if (is_first_line_) {
          is_first_line_ = false;
          continue;
        }

        if (line.empty()) {
          continue;
        }

        row = Implementation::parse_raw_crypto_trade_row(line);
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

        row = Implementation::parse_raw_crypto_trade_tuple(
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

  class RawOptionTradeStreamState {
   public:
    explicit RawOptionTradeStreamState(const std::filesystem::path& path)
        : reader_(path) {}

    RawOptionTradeStreamState(const RawOptionTradeStreamState&) = delete;
    RawOptionTradeStreamState& operator=(const RawOptionTradeStreamState&) = delete;

    RawOptionTradeStreamState(RawOptionTradeStreamState&& other) noexcept
        : reader_(std::move(other.reader_)),
          is_first_line_(other.is_first_line_),
          intern_cache_(std::move(other.intern_cache_)) {}

    RawOptionTradeStreamState& operator=(RawOptionTradeStreamState&& other) noexcept {
      if (this == &other) {
        return *this;
      }

      reader_ = std::move(other.reader_);
      is_first_line_ = other.is_first_line_;
      intern_cache_ = std::move(other.intern_cache_);
      return *this;
    }

    bool next_row(RawOptionTrade& row) {
      std::string_view line;

      while (reader_.template next_line<Specialization>(line)) {
        if (is_first_line_) {
          is_first_line_ = false;
          continue;
        }

        if (line.empty()) {
          continue;
        }

        row = Implementation::parse_raw_option_trade_row(line);
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

        row = Implementation::parse_raw_option_trade_tuple(
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

  class RawOptionQuoteStreamState {
   public:
    explicit RawOptionQuoteStreamState(const std::filesystem::path& path)
        : reader_(path) {}

    RawOptionQuoteStreamState(const RawOptionQuoteStreamState&) = delete;
    RawOptionQuoteStreamState& operator=(const RawOptionQuoteStreamState&) = delete;

    RawOptionQuoteStreamState(RawOptionQuoteStreamState&& other) noexcept
        : reader_(std::move(other.reader_)),
          is_first_line_(other.is_first_line_),
          intern_cache_(std::move(other.intern_cache_)) {}

    RawOptionQuoteStreamState& operator=(RawOptionQuoteStreamState&& other) noexcept {
      if (this == &other) {
        return *this;
      }

      reader_ = std::move(other.reader_);
      is_first_line_ = other.is_first_line_;
      intern_cache_ = std::move(other.intern_cache_);
      return *this;
    }

    bool next_row(RawOptionQuote& row) {
      std::string_view line;

      while (reader_.template next_line<Specialization>(line)) {
        if (is_first_line_) {
          is_first_line_ = false;
          continue;
        }

        if (line.empty()) {
          continue;
        }

        row = Implementation::parse_raw_option_quote_row(line);
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

        row = Implementation::parse_raw_option_quote_tuple(
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

  class CryptoTradeRowsIterator {
   public:
    explicit CryptoTradeRowsIterator(
        const std::filesystem::path& path,
        bool sort_by_participant_timestamp = false) {
      if (sort_by_participant_timestamp) {
        detail::BitsetParseCache<96> bitset_cache;
        rows_ = load_rows<CryptoTrade>(
            path,
            [&bitset_cache](std::string_view line) {
              return Implementation::parse_crypto_trade_row(line, bitset_cache);
            });
        std::stable_sort(
            rows_.begin(),
            rows_.end(),
            [](const CryptoTrade& lhs, const CryptoTrade& rhs) {
              if (lhs.participant_timestamp != rhs.participant_timestamp) {
                return lhs.participant_timestamp < rhs.participant_timestamp;
              }
              if (lhs.ticker != rhs.ticker) {
                return lhs.ticker < rhs.ticker;
              }
              return lhs.id < rhs.id;
            });
      } else {
        stream_state_.emplace(path);
      }
    }

    CryptoTradeRowsIterator& iter() { return *this; }

    CryptoTrade next() {
      return Implementation::template next_parsed_row<CryptoTrade, CryptoTradeStreamState>(
          stream_state_,
          rows_,
          row_index_);
    }

   private:
    std::optional<CryptoTradeStreamState> stream_state_;
    std::vector<CryptoTrade> rows_;
    std::size_t row_index_ = 0;
  };

  class OptionTradeRowsIterator {
   public:
    explicit OptionTradeRowsIterator(
        const std::filesystem::path& path,
        bool sort_by_sip_timestamp = false) {
      static_cast<void>(sort_by_sip_timestamp);
      stream_state_.emplace(path);
    }

    OptionTradeRowsIterator& iter() { return *this; }

    OptionTrade next() {
      return Implementation::template next_parsed_row<OptionTrade, OptionTradeStreamState>(
          stream_state_,
          rows_,
          row_index_);
    }

   private:
    std::optional<OptionTradeStreamState> stream_state_;
    std::vector<OptionTrade> rows_;
    std::size_t row_index_ = 0;
  };

  class OptionQuoteRowsIterator {
   public:
    explicit OptionQuoteRowsIterator(
        const std::filesystem::path& path,
        bool sort_by_sip_timestamp = false) {
      static_cast<void>(sort_by_sip_timestamp);
      stream_state_.emplace(path);
    }

    OptionQuoteRowsIterator& iter() { return *this; }

    OptionQuote next() {
      return Implementation::template next_parsed_row<OptionQuote, OptionQuoteStreamState>(
          stream_state_,
          rows_,
          row_index_);
    }

   private:
    std::optional<OptionQuoteStreamState> stream_state_;
    std::vector<OptionQuote> rows_;
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

  class FuturesTradeRowsIterator {
   public:
    explicit FuturesTradeRowsIterator(const std::filesystem::path& path) {
      stream_state_.emplace(path);
    }

    FuturesTradeRowsIterator& iter() { return *this; }

    FuturesTrade next() {
      return Implementation::template next_parsed_row<FuturesTrade, FuturesTradeStreamState>(
          stream_state_,
          rows_,
          row_index_);
    }

   private:
    std::optional<FuturesTradeStreamState> stream_state_;
    std::vector<FuturesTrade> rows_;
    std::size_t row_index_ = 0;
  };

  class FuturesQuoteRowsIterator {
   public:
    explicit FuturesQuoteRowsIterator(const std::filesystem::path& path) {
      stream_state_.emplace(path);
    }

    FuturesQuoteRowsIterator& iter() { return *this; }

    FuturesQuote next() {
      return Implementation::template next_parsed_row<FuturesQuote, FuturesQuoteStreamState>(
          stream_state_,
          rows_,
          row_index_);
    }

   private:
    std::optional<FuturesQuoteStreamState> stream_state_;
    std::vector<FuturesQuote> rows_;
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

  class RawCryptoTradeRowsIterator {
   public:
    explicit RawCryptoTradeRowsIterator(
        const std::filesystem::path& path,
        bool sort_by_participant_timestamp = false) {
      if (sort_by_participant_timestamp) {
        rows_ = load_rows<RawCryptoTrade>(
            path,
            &Implementation::parse_raw_crypto_trade_row);
        std::stable_sort(
            rows_.begin(),
            rows_.end(),
            [](const RawCryptoTrade& lhs, const RawCryptoTrade& rhs) {
              const auto lhs_participant_timestamp =
                  Specialization::template parse_integer<std::uint64_t>(
                      lhs[4],
                      "participant_timestamp");
              const auto rhs_participant_timestamp =
                  Specialization::template parse_integer<std::uint64_t>(
                      rhs[4],
                      "participant_timestamp");
              if (lhs_participant_timestamp != rhs_participant_timestamp) {
                return lhs_participant_timestamp < rhs_participant_timestamp;
              }
              if (lhs[0] != rhs[0]) {
                return lhs[0] < rhs[0];
              }
              return Specialization::template parse_integer<std::uint64_t>(lhs[3], "id") <
                  Specialization::template parse_integer<std::uint64_t>(rhs[3], "id");
            });
      } else {
        stream_state_.emplace(path);
      }
    }

    RawCryptoTradeRowsIterator(const RawCryptoTradeRowsIterator&) = delete;
    RawCryptoTradeRowsIterator& operator=(const RawCryptoTradeRowsIterator&) = delete;

    RawCryptoTradeRowsIterator(RawCryptoTradeRowsIterator&& other) noexcept
        : stream_state_(std::move(other.stream_state_)),
          rows_(std::move(other.rows_)),
          row_index_(other.row_index_),
          intern_cache_(std::move(other.intern_cache_)) {}

    RawCryptoTradeRowsIterator& operator=(RawCryptoTradeRowsIterator&& other) noexcept {
      if (this == &other) {
        return *this;
      }

      stream_state_ = std::move(other.stream_state_);
      rows_ = std::move(other.rows_);
      row_index_ = other.row_index_;
      intern_cache_ = std::move(other.intern_cache_);
      return *this;
    }

    RawCryptoTradeRowsIterator& iter() { return *this; }

    nanobind::tuple next() {
      if (stream_state_) {
        nanobind::tuple row;
        if (stream_state_->next_tuple(row)) {
          return row;
        }

        stream_state_.reset();
        throw nanobind::stop_iteration();
      }

      return Implementation::raw_crypto_trade_array_to_tuple(rows_next(), intern_cache_);
    }

   private:
    RawCryptoTrade rows_next() {
      if (row_index_ >= rows_.size()) {
        throw nanobind::stop_iteration();
      }
      return std::move(rows_[row_index_++]);
    }

    std::optional<RawCryptoTradeStreamState> stream_state_;
    std::vector<RawCryptoTrade> rows_;
    std::size_t row_index_ = 0;
    detail::RawBytesInternCache intern_cache_;
  };

  class RawOptionTradeRowsIterator {
   public:
    explicit RawOptionTradeRowsIterator(
        const std::filesystem::path& path,
        bool sort_by_sip_timestamp = false) {
      static_cast<void>(sort_by_sip_timestamp);
      stream_state_.emplace(path);
    }

    RawOptionTradeRowsIterator(const RawOptionTradeRowsIterator&) = delete;
    RawOptionTradeRowsIterator& operator=(const RawOptionTradeRowsIterator&) = delete;

    RawOptionTradeRowsIterator(RawOptionTradeRowsIterator&& other) noexcept
        : stream_state_(std::move(other.stream_state_)),
          rows_(std::move(other.rows_)),
          row_index_(other.row_index_),
          intern_cache_(std::move(other.intern_cache_)) {}

    RawOptionTradeRowsIterator& operator=(RawOptionTradeRowsIterator&& other) noexcept {
      if (this == &other) {
        return *this;
      }

      stream_state_ = std::move(other.stream_state_);
      rows_ = std::move(other.rows_);
      row_index_ = other.row_index_;
      intern_cache_ = std::move(other.intern_cache_);
      return *this;
    }

    RawOptionTradeRowsIterator& iter() { return *this; }

    nanobind::tuple next() {
      if (stream_state_) {
        nanobind::tuple row;
        if (stream_state_->next_tuple(row)) {
          return row;
        }

        stream_state_.reset();
        throw nanobind::stop_iteration();
      }

      return Implementation::raw_option_trade_array_to_tuple(rows_next(), intern_cache_);
    }

   private:
    RawOptionTrade rows_next() {
      if (row_index_ >= rows_.size()) {
        throw nanobind::stop_iteration();
      }
      return std::move(rows_[row_index_++]);
    }

    std::optional<RawOptionTradeStreamState> stream_state_;
    std::vector<RawOptionTrade> rows_;
    std::size_t row_index_ = 0;
    detail::RawBytesInternCache intern_cache_;
  };

  class RawOptionQuoteRowsIterator {
   public:
    explicit RawOptionQuoteRowsIterator(
        const std::filesystem::path& path,
        bool sort_by_sip_timestamp = false) {
      static_cast<void>(sort_by_sip_timestamp);
      stream_state_.emplace(path);
    }

    RawOptionQuoteRowsIterator(const RawOptionQuoteRowsIterator&) = delete;
    RawOptionQuoteRowsIterator& operator=(const RawOptionQuoteRowsIterator&) = delete;

    RawOptionQuoteRowsIterator(RawOptionQuoteRowsIterator&& other) noexcept
        : stream_state_(std::move(other.stream_state_)),
          rows_(std::move(other.rows_)),
          row_index_(other.row_index_),
          intern_cache_(std::move(other.intern_cache_)) {}

    RawOptionQuoteRowsIterator& operator=(RawOptionQuoteRowsIterator&& other) noexcept {
      if (this == &other) {
        return *this;
      }

      stream_state_ = std::move(other.stream_state_);
      rows_ = std::move(other.rows_);
      row_index_ = other.row_index_;
      intern_cache_ = std::move(other.intern_cache_);
      return *this;
    }

    RawOptionQuoteRowsIterator& iter() { return *this; }

    nanobind::tuple next() {
      if (stream_state_) {
        nanobind::tuple row;
        if (stream_state_->next_tuple(row)) {
          return row;
        }

        stream_state_.reset();
        throw nanobind::stop_iteration();
      }

      return Implementation::raw_option_quote_array_to_tuple(rows_next(), intern_cache_);
    }

   private:
    RawOptionQuote rows_next() {
      if (row_index_ >= rows_.size()) {
        throw nanobind::stop_iteration();
      }
      return std::move(rows_[row_index_++]);
    }

    std::optional<RawOptionQuoteStreamState> stream_state_;
    std::vector<RawOptionQuote> rows_;
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
      std::string_view record_type,
      bool force = false) {
    if (record_type == "stock_trade") {
      detail::BitsetParseCache<96> bitset_cache;
      return build_parsed_database<StockTrade>(
          input_path,
          database_path,
          record_type,
          [&bitset_cache](std::string_view line) {
            return Implementation::parse_trade_row(line, bitset_cache);
          },
          [](const StockTrade& row) { return row.sip_timestamp; },
          force);
    }

    if (record_type == "crypto_trade") {
      detail::BitsetParseCache<96> bitset_cache;
      return build_parsed_database<CryptoTrade>(
          input_path,
          database_path,
          record_type,
          [&bitset_cache](std::string_view line) {
            return Implementation::parse_crypto_trade_row(line, bitset_cache);
          },
          [](const CryptoTrade& row) { return row.participant_timestamp; },
          force,
          true);
    }

    if (record_type == "option_trade") {
      return build_option_database<OptionTrade>(
          input_path,
          database_path,
          record_type,
          [](std::string_view line) {
            return Implementation::parse_option_trade_row(line);
          },
          force);
    }

    if (record_type == "option_quote") {
      return build_option_database<OptionQuote>(
          input_path,
          database_path,
          record_type,
          [](std::string_view line) {
            return Implementation::parse_option_quote_row(line);
          },
          force);
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
          [](const StockQuote& row) { return row.sip_timestamp; },
          force);
    }

    if (record_type == "currency_quote") {
      return build_parsed_database<CurrencyQuote>(
          input_path,
          database_path,
          record_type,
          [](std::string_view line) {
            return Implementation::parse_currency_quote_row(line);
          },
          [](const CurrencyQuote& row) { return row.participant_timestamp; },
          force);
    }

    if (record_type == "future_trade" ||
        record_type == "future_cbot_trade" ||
        record_type == "future_cme_trade" ||
        record_type == "future_comex_trade" ||
        record_type == "future_nymex_trade") {
      return build_parsed_database<FuturesTrade>(
          input_path,
          database_path,
          record_type,
          [](std::string_view line) {
            return Implementation::parse_futures_trade_row(line);
          },
          [](const FuturesTrade& row) { return row.timestamp; },
          force);
    }

    if (record_type == "future_quote" ||
        record_type == "future_cbot_quote" ||
        record_type == "future_cme_quote" ||
        record_type == "future_comex_quote" ||
        record_type == "future_nymex_quote") {
      return build_parsed_database<FuturesQuote>(
          input_path,
          database_path,
          record_type,
          [](std::string_view line) {
            return Implementation::parse_futures_quote_row(line);
          },
          [](const FuturesQuote& row) { return row.timestamp; },
          force);
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
      DateTimestampFn date_timestamp,
      bool force,
      bool validate_ticker_timestamp_order = false) {
    std::filesystem::create_directories(database_path);

    detail::BufferedGzipLineReader reader(input_path);
    detail::AtomicBinaryRecordWriter writer;
    std::string_view line;
    const std::filesystem::path output_root =
        database_path / std::string(record_type) /
        detail::date_directory_name_from_filename(input_path);
    std::string current_ticker;
    bool is_first_line = true;
    bool has_current_output = false;
    bool skip_current_output = false;
    bool has_previous_row = false;
    std::string previous_ticker;
    std::uint64_t previous_timestamp = 0;
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
      const std::uint64_t row_timestamp = date_timestamp(row);
      if (validate_ticker_timestamp_order) {
        if (has_previous_row) {
          if (row.ticker < previous_ticker) {
            std::ostringstream message;
            message << "input rows are not ordered by ticker,participant_timestamp: "
                    << "ticker '" << row.ticker << "' appeared after ticker '"
                    << previous_ticker << "'";
            throw std::invalid_argument(message.str());
          }
          if (row.ticker == previous_ticker && row_timestamp < previous_timestamp) {
            std::ostringstream message;
            message << "input rows are not ordered by ticker,participant_timestamp: "
                    << "participant_timestamp " << row_timestamp
                    << " appeared after " << previous_timestamp
                    << " for ticker '" << row.ticker << "'";
            throw std::invalid_argument(message.str());
          }
        }
        previous_ticker = row.ticker;
        previous_timestamp = row_timestamp;
        has_previous_row = true;
      }

      std::filesystem::create_directories(output_root);

      if (!has_current_output || row.ticker != current_ticker) {
        writer.commit();
        current_ticker = row.ticker;
        const auto output_path = output_root / current_ticker;
        skip_current_output = !force && std::filesystem::exists(output_path);
        if (!skip_current_output) {
          writer.open(output_path);
        }
        has_current_output = true;
      }

      if (skip_current_output) {
        continue;
      }

      writer.write(row.pack());
      ++rows_written;
    }

    writer.commit();
    return rows_written;
  }

  template <typename RowType, typename ParseRowFn>
  static std::uint64_t build_option_database(
      const std::filesystem::path& input_path,
      const std::filesystem::path& database_path,
      std::string_view record_type,
      ParseRowFn parse_row,
      bool force) {
    std::filesystem::create_directories(database_path);

    detail::BufferedGzipLineReader reader(input_path);
    std::string_view line;
    const std::filesystem::path output_root =
        database_path / std::string(record_type) /
        detail::date_directory_name_from_filename(input_path);
    bool is_first_line = true;
    std::string current_root;
    std::unordered_map<std::string, std::vector<RowType>> rows_by_key;
    std::vector<std::string> key_order;
    std::uint64_t rows_written = 0;

    const auto flush_root = [&]() {
      if (rows_by_key.empty()) {
        return;
      }

      std::filesystem::create_directories(output_root);

      for (const std::string& row_key : key_order) {
        auto found = rows_by_key.find(row_key);
        if (found == rows_by_key.end()) {
          continue;
        }

        auto& rows = found->second;
        std::stable_sort(
            rows.begin(),
            rows.end(),
            [](const RowType& lhs, const RowType& rhs) {
              return lhs.sip_timestamp < rhs.sip_timestamp;
            });

        const auto output_path = output_root / row_key;
        std::filesystem::create_directories(output_path.parent_path());
        if (!force && std::filesystem::exists(output_path)) {
          continue;
        }

        detail::AtomicBinaryRecordWriter writer;
        writer.open(output_path);
        for (const RowType& row : rows) {
          writer.write(row.pack());
          ++rows_written;
        }
        writer.commit();
      }

      rows_by_key.clear();
      key_order.clear();
    };

    while (reader.template next_line<Specialization>(line)) {
      if (is_first_line) {
        is_first_line = false;
        continue;
      }

      if (line.empty()) {
        continue;
      }

      RowType row = parse_row(line);
      if (!current_root.empty() && row.root != current_root) {
        flush_root();
      }
      if (current_root.empty() || rows_by_key.empty()) {
        current_root = row.root;
      }

      const std::string row_key = detail::option_contract_key(
          row.root,
          row.expiration,
          row.right,
          row.strike_millis);
      auto [found, inserted] = rows_by_key.try_emplace(row_key);
      if (inserted) {
        key_order.push_back(row_key);
      }
      found->second.push_back(std::move(row));
    }

    flush_root();
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
    result.exact_size = detail::parse_decimal_quantity(
        cursor.template next_field<Specialization, true>(scratch),
        "size");
    result.size = detail::decimal_quantity_to_double(result.exact_size);
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

  static CryptoTrade parse_crypto_trade_row(
      std::string_view line,
      detail::BitsetParseCache<96>& bitset_cache) {
    std::size_t cursor = 0;
    CryptoTrade result;

    result.ticker.assign(next_raw_unquoted_field<true>(line, cursor));
    result.conditions = bitset_cache.get_or_parse(
        next_raw_unquoted_field<true>(line, cursor),
        "conditions");
    result.exchange =
        Specialization::template parse_integer<std::uint16_t>(
            next_raw_unquoted_field<true>(line, cursor),
            "exchange");
    result.id =
        Specialization::template parse_integer<std::uint64_t>(
            next_raw_unquoted_field<true>(line, cursor),
            "id");
    result.participant_timestamp =
        Specialization::template parse_integer<std::uint64_t>(
            next_raw_unquoted_field<true>(line, cursor),
            "participant_timestamp");
    result.price = Specialization::parse_double(
        next_raw_unquoted_field<true>(line, cursor),
        "price");
    result.size = Specialization::parse_double(
        next_raw_unquoted_field<false>(line, cursor),
        "size");

    finish_raw_row(line, cursor);
    return result;
  }

  static RawCryptoTrade parse_raw_crypto_trade_row(std::string_view line) {
    std::size_t cursor = 0;
    RawCryptoTrade result;
    result[0].assign(next_raw_unquoted_field<true>(line, cursor));
    result[1].assign(next_raw_unquoted_field<true>(line, cursor));
    result[2].assign(next_raw_unquoted_field<true>(line, cursor));
    result[3].assign(next_raw_unquoted_field<true>(line, cursor));
    result[4].assign(next_raw_unquoted_field<true>(line, cursor));
    result[5].assign(next_raw_unquoted_field<true>(line, cursor));
    result[6].assign(next_raw_unquoted_field<false>(line, cursor));
    finish_raw_row(line, cursor);
    return result;
  }

  static nanobind::tuple parse_raw_crypto_trade_tuple(
      std::string_view line,
      detail::RawBytesInternCache& intern_cache) {
    std::size_t cursor = 0;
    nanobind::tuple result = make_raw_tuple<7>();

    set_raw_ticker_field(
        result,
        next_raw_unquoted_field<true>(line, cursor),
        intern_cache);
    set_raw_small_uint_field(
        result,
        1,
        next_raw_unquoted_field<true>(line, cursor),
        intern_cache);
    set_raw_small_uint_field(
        result,
        2,
        next_raw_unquoted_field<true>(line, cursor),
        intern_cache);
    set_raw_bytes_field(result, 3, next_raw_unquoted_field<true>(line, cursor));
    set_raw_bytes_field(result, 4, next_raw_unquoted_field<true>(line, cursor));
    set_raw_bytes_field(result, 5, next_raw_unquoted_field<true>(line, cursor));
    set_raw_bytes_field(result, 6, next_raw_unquoted_field<false>(line, cursor));

    finish_raw_row(line, cursor);
    return result;
  }

  static nanobind::tuple raw_crypto_trade_array_to_tuple(
      const RawCryptoTrade& fields,
      detail::RawBytesInternCache& intern_cache) {
    nanobind::tuple result = make_raw_tuple<7>();
    set_raw_ticker_field(result, fields[0], intern_cache);
    set_raw_small_uint_field(result, 1, fields[1], intern_cache);
    set_raw_small_uint_field(result, 2, fields[2], intern_cache);
    set_raw_bytes_field(result, 3, fields[3]);
    set_raw_bytes_field(result, 4, fields[4]);
    set_raw_bytes_field(result, 5, fields[5]);
    set_raw_bytes_field(result, 6, fields[6]);
    return result;
  }

  static OptionTrade parse_option_trade_row(std::string_view line) {
    detail::CsvLineCursor cursor(line);
    std::string scratch;
    OptionTrade result;

    result.assign_symbol(cursor.template next_field<Specialization, true>(scratch));
    result.conditions = detail::parse_option_condition_bits(
        cursor.template next_field<Specialization, true>(scratch),
        "conditions");
    result.correction =
        Specialization::template parse_integer<std::int32_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "correction");
    result.exchange =
        Specialization::template parse_integer<std::uint16_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "exchange");
    result.price = Specialization::parse_double(
        cursor.template next_field<Specialization, true>(scratch),
        "price");
    result.sip_timestamp =
        Specialization::template parse_integer<std::uint64_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "sip_timestamp");
    result.size =
        Specialization::template parse_integer<std::uint32_t>(
            cursor.template next_field<Specialization, false>(scratch),
            "size");

    cursor.finish();
    return result;
  }

  static RawOptionTrade parse_raw_option_trade_row(std::string_view line) {
    detail::CsvLineCursor cursor(line);
    std::string scratch;
    RawOptionTrade result;
    result[0].assign(cursor.template next_field<Specialization, true>(scratch));
    result[1].assign(cursor.template next_field<Specialization, true>(scratch));
    result[2].assign(cursor.template next_field<Specialization, true>(scratch));
    result[3].assign(cursor.template next_field<Specialization, true>(scratch));
    result[4].assign(cursor.template next_field<Specialization, true>(scratch));
    result[5].assign(cursor.template next_field<Specialization, true>(scratch));
    result[6].assign(cursor.template next_field<Specialization, false>(scratch));
    cursor.finish();
    return result;
  }

  static nanobind::tuple parse_raw_option_trade_tuple(
      std::string_view line,
      detail::RawBytesInternCache& intern_cache) {
    detail::CsvLineCursor cursor(line);
    std::string scratch;
    nanobind::tuple result = make_raw_tuple<7>();

    set_raw_ticker_field(
        result,
        cursor.template next_field<Specialization, true>(scratch),
        intern_cache);
    set_raw_bytes_field(result, 1, cursor.template next_field<Specialization, true>(scratch));
    set_raw_small_uint_field(
        result,
        2,
        cursor.template next_field<Specialization, true>(scratch),
        intern_cache);
    set_raw_bytes_field(result, 3, cursor.template next_field<Specialization, true>(scratch));
    set_raw_bytes_field(result, 4, cursor.template next_field<Specialization, true>(scratch));
    set_raw_bytes_field(result, 5, cursor.template next_field<Specialization, true>(scratch));
    set_raw_small_uint_field(
        result,
        6,
        cursor.template next_field<Specialization, false>(scratch),
        intern_cache);

    cursor.finish();
    return result;
  }

  static nanobind::tuple raw_option_trade_array_to_tuple(
      const RawOptionTrade& fields,
      detail::RawBytesInternCache& intern_cache) {
    nanobind::tuple result = make_raw_tuple<7>();
    set_raw_ticker_field(result, fields[0], intern_cache);
    set_raw_bytes_field(result, 1, fields[1]);
    set_raw_small_uint_field(result, 2, fields[2], intern_cache);
    set_raw_bytes_field(result, 3, fields[3]);
    set_raw_bytes_field(result, 4, fields[4]);
    set_raw_bytes_field(result, 5, fields[5]);
    set_raw_small_uint_field(result, 6, fields[6], intern_cache);
    return result;
  }

  static OptionQuote parse_option_quote_row(std::string_view line) {
    std::size_t cursor = 0;
    OptionQuote result;

    result.assign_symbol(next_raw_unquoted_field<true>(line, cursor));
    result.ask_exchange =
        Specialization::template parse_integer<std::uint16_t>(
            next_raw_unquoted_field<true>(line, cursor),
            "ask_exchange");
    result.ask_price = detail::parse_nullable_double(
        next_raw_unquoted_field<true>(line, cursor),
        "ask_price");
    result.ask_size =
        Specialization::template parse_integer<std::uint32_t>(
            next_raw_unquoted_field<true>(line, cursor),
            "ask_size");
    result.bid_exchange =
        Specialization::template parse_integer<std::uint16_t>(
            next_raw_unquoted_field<true>(line, cursor),
            "bid_exchange");
    result.bid_price = detail::parse_nullable_double(
        next_raw_unquoted_field<true>(line, cursor),
        "bid_price");
    result.bid_size =
        Specialization::template parse_integer<std::uint32_t>(
            next_raw_unquoted_field<true>(line, cursor),
            "bid_size");
    result.sequence_number =
        Specialization::template parse_integer<std::uint64_t>(
            next_raw_unquoted_field<true>(line, cursor),
            "sequence_number");
    result.sip_timestamp =
        Specialization::template parse_integer<std::uint64_t>(
            next_raw_unquoted_field<false>(line, cursor),
            "sip_timestamp");

    finish_raw_row(line, cursor);
    return result;
  }

  static RawOptionQuote parse_raw_option_quote_row(std::string_view line) {
    std::size_t cursor = 0;
    RawOptionQuote result;
    result[0].assign(next_raw_unquoted_field<true>(line, cursor));
    result[1].assign(next_raw_unquoted_field<true>(line, cursor));
    result[2].assign(next_raw_unquoted_field<true>(line, cursor));
    result[3].assign(next_raw_unquoted_field<true>(line, cursor));
    result[4].assign(next_raw_unquoted_field<true>(line, cursor));
    result[5].assign(next_raw_unquoted_field<true>(line, cursor));
    result[6].assign(next_raw_unquoted_field<true>(line, cursor));
    result[7].assign(next_raw_unquoted_field<true>(line, cursor));
    result[8].assign(next_raw_unquoted_field<false>(line, cursor));
    finish_raw_row(line, cursor);
    return result;
  }

  static nanobind::tuple parse_raw_option_quote_tuple(
      std::string_view line,
      detail::RawBytesInternCache& intern_cache) {
    std::size_t cursor = 0;
    nanobind::tuple result = make_raw_tuple<9>();

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
    set_raw_small_uint_field(
        result,
        4,
        next_raw_unquoted_field<true>(line, cursor),
        intern_cache);
    set_raw_bytes_field(result, 5, next_raw_unquoted_field<true>(line, cursor));
    set_raw_small_uint_field(
        result,
        6,
        next_raw_unquoted_field<true>(line, cursor),
        intern_cache);
    set_raw_bytes_field(result, 7, next_raw_unquoted_field<true>(line, cursor));
    set_raw_bytes_field(result, 8, next_raw_unquoted_field<false>(line, cursor));

    finish_raw_row(line, cursor);
    return result;
  }

  static nanobind::tuple raw_option_quote_array_to_tuple(
      const RawOptionQuote& fields,
      detail::RawBytesInternCache& intern_cache) {
    nanobind::tuple result = make_raw_tuple<9>();
    set_raw_ticker_field(result, fields[0], intern_cache);
    set_raw_small_uint_field(result, 1, fields[1], intern_cache);
    set_raw_bytes_field(result, 2, fields[2]);
    set_raw_small_uint_field(result, 3, fields[3], intern_cache);
    set_raw_small_uint_field(result, 4, fields[4], intern_cache);
    set_raw_bytes_field(result, 5, fields[5]);
    set_raw_small_uint_field(result, 6, fields[6], intern_cache);
    set_raw_bytes_field(result, 7, fields[7]);
    set_raw_bytes_field(result, 8, fields[8]);
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

  static FuturesTrade parse_futures_trade_row(std::string_view line) {
    detail::CsvLineCursor cursor(line);
    std::string scratch;
    FuturesTrade result;

    result.ticker.assign(cursor.template next_field<Specialization, true>(scratch));
    result.timestamp =
        Specialization::template parse_integer<std::uint64_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "timestamp");
    result.sequence_number =
        Specialization::template parse_integer<std::uint64_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "sequence_number");
    result.report_sequence =
        Specialization::template parse_integer<std::uint32_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "report_sequence");
    result.price = Specialization::parse_double(
        cursor.template next_field<Specialization, true>(scratch),
        "price");
    result.size =
        Specialization::template parse_integer<std::uint32_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "size");
    result.correction =
        Specialization::template parse_integer<std::int32_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "correction");
    result.exchange =
        Specialization::template parse_integer<std::uint16_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "exchange");
    static_cast<void>(cursor.template next_field<Specialization, false>(scratch));

    cursor.finish();
    return result;
  }

  static FuturesQuote parse_futures_quote_row(std::string_view line) {
    detail::CsvLineCursor cursor(line);
    std::string scratch;
    FuturesQuote result;

    result.ticker.assign(cursor.template next_field<Specialization, true>(scratch));
    result.timestamp =
        Specialization::template parse_integer<std::uint64_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "timestamp");
    result.sequence_number =
        Specialization::template parse_integer<std::uint64_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "sequence_number");
    result.report_sequence =
        Specialization::template parse_integer<std::uint32_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "report_sequence");
    result.ask_timestamp =
        Specialization::template parse_integer<std::uint64_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "ask_timestamp");
    result.ask_price = detail::parse_nullable_double(
        cursor.template next_field<Specialization, true>(scratch),
        "ask_price");
    result.ask_size =
        Specialization::template parse_integer<std::uint32_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "ask_size");
    result.bid_timestamp =
        Specialization::template parse_integer<std::uint64_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "bid_timestamp");
    result.bid_price = detail::parse_nullable_double(
        cursor.template next_field<Specialization, true>(scratch),
        "bid_price");
    result.bid_size =
        Specialization::template parse_integer<std::uint32_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "bid_size");
    result.exchange =
        Specialization::template parse_integer<std::uint16_t>(
            cursor.template next_field<Specialization, true>(scratch),
            "exchange");
    static_cast<void>(cursor.template next_field<Specialization, false>(scratch));

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

  static std::uint64_t participant_timestamp_value(const RawOptionTrade& row) {
    return Specialization::template parse_integer<std::uint64_t>(
        row[5],
        "sip_timestamp");
  }

  static std::uint64_t participant_timestamp_value(const RawOptionQuote& row) {
    return Specialization::template parse_integer<std::uint64_t>(
        row[8],
        "sip_timestamp");
  }

  static std::uint64_t participant_timestamp_value(const OptionTrade& row) {
    return row.sip_timestamp;
  }

  static std::uint64_t participant_timestamp_value(const OptionQuote& row) {
    return row.sip_timestamp;
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

  static std::uint64_t sip_timestamp_value(const RawOptionTrade& row) {
    return participant_timestamp_value(row);
  }

  static std::uint64_t sip_timestamp_value(const RawOptionQuote& row) {
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

  static const std::string& ticker_value(const RawOptionTrade& row) {
    return row[0];
  }

  static const std::string& ticker_value(const RawOptionQuote& row) {
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
