#pragma once

#include "framework/app_context.h"
#include "framework/module.h"
#include "framework/widgets.h"

#include <st/orders/order_store.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace st::app {

/// Spare part orders. Window: "Bestellungen".
class OrdersModule final : public Module {
public:
  std::string_view name() const override { return "orders"; }
  bool available(const AppContext &ctx) const override;
  void init(AppContext &ctx) override;
  void update(AppContext &ctx) override;

private:
  void drawWindow(AppContext &ctx);
  /// Editor for one order. `isNew` shows customer selection and disables saving per field.
  void drawOrder(AppContext &ctx, orders::Order &order, bool isNew);
  /// List of orders; inline mode draws alternating stripes behind the entries.
  void drawOrderList(AppContext &ctx, const std::vector<std::uint64_t> &ids);
  void drawOrderEntry(AppContext &ctx, std::uint64_t id);
  /// Customers from the settings plus all customers of existing orders.
  std::vector<std::string> knownCustomers(const AppContext &ctx) const;
  orders::Order &draftOf(const orders::Order &stored);
  void drawNewOrderPopup(AppContext &ctx);
  void drawExportPopup(AppContext &ctx);
  void save(AppContext &ctx, orders::Order order);
  void updateAttention(AppContext &ctx);

  orders::OrderStore mStore;
  std::map<std::uint64_t, orders::Order> mDrafts;
  PollTimer mPoll;
  std::string mSearch;
  std::vector<std::uint64_t> mSearchResult;
  orders::Order mNewOrder;
  ui::AutocompleteState mCustomerInput;
  std::uint64_t mCancelId = 0;
  std::string mCancelNote;
  std::uint64_t mPopupId = 0;
  std::string mExportFile;
};

} // namespace st::app
