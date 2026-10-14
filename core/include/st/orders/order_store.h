#pragma once

#include "st/util/fs.h"
#include "st/util/json.h"
#include "st/util/session.h"

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

/// Spare part orders ("Bestellungen").
///
/// Workflow: received -> booked in Sivas -> prepared for shipping -> shipped -> finished.
/// An order can be cancelled at any time before it is finished. The state is derived
/// from the dates that are filled in. File format: {"Bestellungen": [ ... ]} with
/// German keys (see docs/dateiformate.md).
namespace st::orders {

enum class OrderState { Received,
                        Booked,
                        Prepared,
                        Shipped,
                        Finished,
                        Cancelled };

inline constexpr std::array kAllStates = {OrderState::Received, OrderState::Booked, OrderState::Prepared,
                                          OrderState::Shipped, OrderState::Finished, OrderState::Cancelled};

/// German label of a state ("Eingegangen", "Sivas erfasst", "Versandbereit", ...).
std::string_view stateLabel(OrderState state);

struct Order {
  std::uint64_t id = 0;
  std::string orderNumber;
  std::string billNumber;
  /// Timestamp of the last change; used to merge concurrent edits.
  std::string changeTime;
  std::vector<std::string> files;

  std::string entryDate;
  std::string bookedDate;
  std::string preparedDate;
  std::string shippingDate;
  std::string finishedDate;

  std::string cancelledDate;
  std::string cancelledNote;

  double price = 0.0;
  bool express = false;
  std::string customer;
  /// Keys of other program versions (kept when saving).
  util::json::Json extra = util::json::Json::object();

  OrderState state() const;
  /// Days from entry to finish (0 if not finished or dates invalid).
  int processingDays() const;
  bool operator==(const Order &) const = default;
};

Order orderFromJson(const util::json::Json &j);
util::json::Json orderToJson(const Order &order);

/// Missing mandatory fields for a new order (empty = valid).
std::vector<std::string> missingFields(const Order &order);

class OrderStore {
public:
  OrderStore() = default;
  explicit OrderStore(util::fs::path file) : mFile(std::move(file)) {}

  const util::fs::path &file() const { return mFile; }

  /// Loads all orders (replaces the in memory state).
  util::Status load();

  /// Merges orders changed by other users since the last load/save.
  /// Returns true if anything changed.
  util::Result<bool> refresh();

  /// Creates (id == 0) or updates an order, stamps changeTime and saves.
  /// Returns the stored order (with its id).
  util::Result<Order> upsert(Order order, const util::Session &session);

  const std::map<std::uint64_t, Order> &orders() const { return mOrders; }
  const Order *find(std::uint64_t id) const;
  /// Ids of orders in `state`, sorted by entry date (newest first).
  std::vector<std::uint64_t> idsInState(OrderState state) const;
  std::size_t countInState(OrderState state) const;
  /// Case insensitive search in customer, order number, bill number and cancel note.
  std::vector<std::uint64_t> search(std::string_view text) const;
  /// All customers that appear in orders, sorted.
  std::vector<std::string> customers() const;

private:
  util::Result<bool> mergeFromDisk();
  util::Status write() const;

  util::fs::path mFile;
  std::map<std::uint64_t, Order> mOrders;
  util::json::Json mRawRoot = util::json::Json::object();
  std::int64_t mLastWrite = 0;
};

} // namespace st::orders
