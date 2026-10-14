#include "st/orders/order_store.h"

#include "st/util/strings.h"
#include "st/util/time.h"

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <random>
#include <set>

namespace st::orders {

using util::json::Json;

namespace {
constexpr const char *kRootKey = "Bestellungen";
constexpr const char *kKnownKeys[] = {"id", "Bestellnummer", "Rechnungsnummer", "Eingangsdatum", "Versanddatum",
                                      "Letzte Änderung", "Dateien", "Sivas erfasst Datum", "Gerichtet Datum",
                                      "Abgeschlossen Datum", "Storniert Datum", "Storniert Notiz", "Preis",
                                      "Expressversand", "Kunde"};

std::uint64_t randomId() {
  static std::mt19937_64 gen{std::random_device{}()};
  std::uint64_t id = 0;
  while (id == 0)
    id = gen();
  return id;
}
} // namespace

std::string_view stateLabel(OrderState state) {
  switch (state) {
  case OrderState::Received: return "Eingegangen";
  case OrderState::Booked: return "Sivas erfasst";
  case OrderState::Prepared: return "Versandbereit";
  case OrderState::Shipped: return "Versendet";
  case OrderState::Finished: return "Erledigt";
  case OrderState::Cancelled: return "Storniert";
  }
  return "";
}

OrderState Order::state() const {
  if (!cancelledDate.empty())
    return OrderState::Cancelled;
  if (!finishedDate.empty())
    return OrderState::Finished;
  if (!shippingDate.empty())
    return OrderState::Shipped;
  if (!preparedDate.empty())
    return OrderState::Prepared;
  if (!bookedDate.empty())
    return OrderState::Booked;
  return OrderState::Received;
}

int Order::processingDays() const {
  if (finishedDate.empty())
    return 0;
  const auto days = util::daysBetween(entryDate, finishedDate);
  return days ? static_cast<int>(std::abs(*days)) : 0;
}

Order orderFromJson(const Json &j) {
  Order o;
  o.id = util::json::uinteger(j, "id", 0);
  o.orderNumber = util::json::str(j, "Bestellnummer");
  o.billNumber = util::json::str(j, "Rechnungsnummer");
  o.entryDate = util::json::str(j, "Eingangsdatum");
  o.shippingDate = util::json::str(j, "Versanddatum");
  o.changeTime = util::json::str(j, "Letzte Änderung");
  o.files = util::json::strings(j, "Dateien");
  o.bookedDate = util::json::str(j, "Sivas erfasst Datum");
  o.preparedDate = util::json::str(j, "Gerichtet Datum");
  o.finishedDate = util::json::str(j, "Abgeschlossen Datum");
  o.cancelledDate = util::json::str(j, "Storniert Datum");
  o.cancelledNote = util::json::str(j, "Storniert Notiz");
  o.price = util::json::number(j, "Preis");
  o.express = util::json::boolean(j, "Expressversand");
  o.customer = util::json::str(j, "Kunde");
  if (j.is_object())
    for (auto it = j.begin(); it != j.end(); ++it)
      if (std::find(std::begin(kKnownKeys), std::end(kKnownKeys), it.key()) == std::end(kKnownKeys))
        o.extra[it.key()] = it.value();
  return o;
}

Json orderToJson(const Order &o) {
  Json j = o.extra.is_object() ? o.extra : Json::object();
  j["id"] = o.id;
  j["Bestellnummer"] = o.orderNumber;
  j["Rechnungsnummer"] = o.billNumber;
  j["Eingangsdatum"] = o.entryDate;
  j["Versanddatum"] = o.shippingDate;
  j["Letzte Änderung"] = o.changeTime;
  j["Dateien"] = o.files;
  j["Sivas erfasst Datum"] = o.bookedDate;
  j["Gerichtet Datum"] = o.preparedDate;
  j["Abgeschlossen Datum"] = o.finishedDate;
  j["Storniert Datum"] = o.cancelledDate;
  j["Storniert Notiz"] = o.cancelledNote;
  j["Preis"] = o.price;
  j["Expressversand"] = o.express;
  j["Kunde"] = o.customer;
  return j;
}

std::vector<std::string> missingFields(const Order &order) {
  std::vector<std::string> out;
  if (util::trim(order.orderNumber).empty())
    out.emplace_back("Bestellnummer");
  if (util::trim(order.customer).empty())
    out.emplace_back("Kunde");
  if (order.entryDate.empty())
    out.emplace_back("Eingangsdatum");
  return out;
}

util::Status OrderStore::load() {
  mOrders.clear();
  mRawRoot = Json::object();
  mLastWrite = 0;
  auto merged = mergeFromDisk();
  if (!merged)
    return std::unexpected(merged.error());
  return {};
}

util::Result<bool> OrderStore::mergeFromDisk() {
  if (!util::pathExists(mFile))
    return false;
  const auto writeTime = util::lastWriteTime(mFile);
  auto j = util::json::load(mFile);
  if (!j)
    return std::unexpected(j.error());
  if (!j->is_object())
    return std::unexpected(util::Error{"Bestellungen haben ein falsches Format: " + util::pathToUtf8(mFile)});
  mRawRoot = *j;
  mRawRoot.erase(kRootKey);
  bool changed = false;
  for (const auto &entry : util::json::array(*j, kRootKey)) {
    Order order = orderFromJson(entry);
    if (order.id == 0)
      continue;
    auto it = mOrders.find(order.id);
    if (it == mOrders.end() || order.changeTime > it->second.changeTime) {
      mOrders[order.id] = std::move(order);
      changed = true;
    }
  }
  mLastWrite = writeTime;
  return changed;
}

util::Result<bool> OrderStore::refresh() {
  if (util::lastWriteTime(mFile) == mLastWrite)
    return false;
  return mergeFromDisk();
}

util::Status OrderStore::write() const {
  Json root = mRawRoot.is_object() ? mRawRoot : Json::object();
  root[kRootKey] = Json::array();
  for (const auto &[id, order] : mOrders)
    root[kRootKey].push_back(orderToJson(order));
  return util::json::save(mFile, root);
}

util::Result<Order> OrderStore::upsert(Order order, const util::Session &session) {
  if (auto merged = mergeFromDisk(); !merged)
    return std::unexpected(merged.error());
  if (order.id == 0) {
    do {
      order.id = randomId();
    } while (mOrders.contains(order.id));
  }
  order.changeTime = session.timestamp();
  mOrders[order.id] = order;
  if (auto s = write(); !s)
    return std::unexpected(s.error());
  mLastWrite = util::lastWriteTime(mFile);
  return order;
}

const Order *OrderStore::find(std::uint64_t id) const {
  const auto it = mOrders.find(id);
  return it == mOrders.end() ? nullptr : &it->second;
}

std::vector<std::uint64_t> OrderStore::idsInState(OrderState state) const {
  std::vector<const Order *> list;
  for (const auto &[id, order] : mOrders)
    if (order.state() == state)
      list.push_back(&order);
  std::sort(list.begin(), list.end(), [](const Order *a, const Order *b) {
    if (a->entryDate != b->entryDate)
      return a->entryDate > b->entryDate;
    return a->orderNumber < b->orderNumber;
  });
  std::vector<std::uint64_t> out;
  for (const auto *o : list)
    out.push_back(o->id);
  return out;
}

std::size_t OrderStore::countInState(OrderState state) const {
  return static_cast<std::size_t>(std::count_if(mOrders.begin(), mOrders.end(), [&](const auto &p) { return p.second.state() == state; }));
}

std::vector<std::uint64_t> OrderStore::search(std::string_view text) const {
  std::vector<std::uint64_t> out;
  if (util::trim(text).empty())
    return out;
  for (const auto &[id, o] : mOrders) {
    if (util::containsIgnoreCase(o.customer, text) || util::containsIgnoreCase(o.orderNumber, text) ||
        util::containsIgnoreCase(o.billNumber, text) || util::containsIgnoreCase(o.cancelledNote, text))
      out.push_back(id);
  }
  return out;
}

std::vector<std::string> OrderStore::customers() const {
  std::set<std::string> set;
  for (const auto &[id, o] : mOrders)
    if (!o.customer.empty())
      set.insert(o.customer);
  return {set.begin(), set.end()};
}

} // namespace st::orders
