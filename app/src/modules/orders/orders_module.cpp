#include "modules/orders/orders_module.h"

#include "framework/platform.h"
#include "framework/widgets.h"

#include <st/alerts/alert_center.h>
#include <st/exporting/reports.h>
#include <st/exporting/xlsx.h>
#include <st/users/user_store.h>
#include <st/util/strings.h>

#include <imgui.h>
#include <imgui/ImGui_helpers.h>
#include <misc/cpp/imgui_stdlib.h>

#include <optional>

namespace st::app {

using orders::Order;
using orders::OrderState;

namespace {
constexpr const char *kWindow = "Bestellungen";
constexpr const char *kDateFormat = "%Y.%m.%d";
/// Notification channel id – stored in the preferences, never rename.
constexpr const char *kAlertPrepared = "orders.prepared";

/// Sales gets a notification as soon as an order is ready for shipping.
alerts::Channel preparedChannel() {
  alerts::Channel channel;
  channel.id = kAlertPrepared;
  channel.label = "Bestellungen versandbereit";
  channel.title = "ServiceTracker - Bestellungen";
  channel.target = kWindow;
  channel.audience = {std::string(users::groups::Sales)};
  channel.summaryText = [](std::size_t count) { return alerts::countSentence(count, "Bestellung", "Bestellungen", "versandbereit"); };
  return channel;
}

ImVec4 stateColor(OrderState state) {
  switch (state) {
  case OrderState::Received: return ImVec4(0.60f, 0.70f, 0.95f, 1.0f);
  case OrderState::Booked: return ImVec4(0.55f, 0.80f, 0.90f, 1.0f);
  case OrderState::Prepared: return ImVec4(0.95f, 0.75f, 0.30f, 1.0f);
  case OrderState::Shipped: return ImVec4(0.80f, 0.60f, 0.95f, 1.0f);
  case OrderState::Finished: return ImVec4(0.45f, 0.80f, 0.45f, 1.0f);
  case OrderState::Cancelled: return ImVec4(0.65f, 0.65f, 0.65f, 1.0f);
  }
  return ImVec4(1, 1, 1, 1);
}

/// Date row: picker + "Heute" button + label. Returns true when changed.
bool dateRow(const char *id, const char *label, std::string &date, const std::string &today) {
  ui::ScopedId scope(id);
  bool changed = ImGui::DatePicker("##date", date, false, 0.0f, 0.0f, kDateFormat);
  if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && !date.empty()) {
    date.clear();
    changed = true;
  }
  ImGui::SetItemTooltip("Rechtsklick entfernt das Datum");
  changed |= ui::todayButton("today", date, today);
  ImGui::SameLine();
  ImGui::TextUnformatted(label);
  return changed;
}

/// Next date field of the workflow that is still empty (nullptr if none).
std::string *nextStep(Order &o, const char **label) {
  if (o.bookedDate.empty()) {
    *label = "Sivas erfasst";
    return &o.bookedDate;
  }
  if (o.preparedDate.empty()) {
    *label = "Versandbereit";
    return &o.preparedDate;
  }
  if (o.shippingDate.empty()) {
    *label = "Versendet";
    return &o.shippingDate;
  }
  return nullptr;
}
} // namespace

bool OrdersModule::available(const AppContext &ctx) const {
  const auto file = util::pathFromUtf8(ctx.paths().ordersFile);
  return (ctx.hasAccess(users::groups::Techsupport) || ctx.hasAccess(users::groups::Sales)) && util::pathExists(file);
}

void OrdersModule::init(AppContext &ctx) {
  mStore = orders::OrderStore(util::pathFromUtf8(ctx.paths().ordersFile));
  ctx.notify.check(mStore.load(), "Bestellungen laden");
  mPoll.setInterval(std::chrono::milliseconds(std::max(500, ctx.settings().refreshIntervalMs)));
  ctx.ui.addWindow(kWindow, [this, &ctx] { drawWindow(ctx); }, true, ImGuiWindowFlags_HorizontalScrollbar);
  ctx.alerts.addChannel(ctx, preparedChannel());
  updateAttention(ctx);
}

void OrdersModule::update(AppContext &ctx) {
  if (!mPoll.due())
    return;
  auto changed = mStore.refresh();
  if (changed && *changed) {
    mSearchResult = mStore.search(mSearch);
    updateAttention(ctx);
  }
}

void OrdersModule::updateAttention(AppContext &ctx) {
  // Orders ready for shipping need attention (legacy behaviour) and notify sales
  const auto prepared = mStore.idsInState(OrderState::Prepared);
  ctx.ui.setAttention(kWindow, !prepared.empty());
  std::vector<alerts::Item> items;
  items.reserve(prepared.size());
  for (const auto id : prepared) {
    const Order *order = mStore.find(id);
    items.push_back({std::to_string(id), order ? order->orderNumber + " (" + order->customer + ")" : std::to_string(id)});
  }
  ctx.alerts.report(kAlertPrepared, items);
}

void OrdersModule::save(AppContext &ctx, Order order) {
  auto stored = mStore.upsert(std::move(order), ctx.session());
  if (ctx.notify.check(stored, "Bestellung speichern")) {
    ctx.alerts.markSeen(kAlertPrepared, std::to_string(stored->id)); // own change is no news
    mSearchResult = mStore.search(mSearch);
    updateAttention(ctx);
  }
}

void OrdersModule::drawOrder(AppContext &ctx, Order &order, bool isNew) {
  const bool cancelled = !order.cancelledDate.empty();
  const std::string today = ctx.session().date();
  bool update = false;
  ui::Disabled locked(cancelled);

  if (!isNew) {
    ImGui::TextDisabled("ID %llu  |  Letzte Änderung: %s", static_cast<unsigned long long>(order.id), ctx.displayTime(order.changeTime).c_str());
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, stateColor(order.state()));
    ImGui::TextUnformatted(std::string(orders::stateLabel(order.state())).c_str());
    ImGui::PopStyleColor();
  }
  if (isNew) {
    ImGui::SetNextItemWidth(200.0f);
    ImGui::InputText("Kunde", &order.customer);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::BeginCombo("##customers", "Bekannte Kunden...")) {
      std::vector<std::string> customers = ctx.settings().knownCustomers;
      for (const auto &c : mStore.customers())
        if (std::find(customers.begin(), customers.end(), c) == customers.end())
          customers.push_back(c);
      for (const auto &c : customers)
        if (ImGui::Selectable(c.c_str(), c == order.customer))
          order.customer = c;
      ImGui::EndCombo();
    }
  } else {
    ImGui::Text("Kunde: %s", order.customer.c_str());
  }

  ImGui::SetNextItemWidth(200.0f);
  ImGui::InputText("Bestellnummer", &order.orderNumber);
  update |= ImGui::IsItemDeactivatedAfterEdit();
  ImGui::SameLine();
  ImGui::Checkbox("Express", &order.express);
  update |= ImGui::IsItemEdited();

  update |= dateRow("entry", "Eingangsdatum", order.entryDate, today);
  update |= dateRow("booked", "Sivas erfasst", order.bookedDate, today);
  update |= dateRow("prepared", "Bestellung in den Versand gestellt", order.preparedDate, today);

  ImGui::SetNextItemWidth(200.0f);
  ImGui::InputText("Rechnungsnummer", &order.billNumber);
  update |= ImGui::IsItemDeactivatedAfterEdit();
  ImGui::SameLine();
  ImGui::SetNextItemWidth(120.0f);
  ImGui::InputDouble("Preis (EUR)", &order.price, 0.0, 0.0, "%.2f");
  update |= ImGui::IsItemDeactivatedAfterEdit();
  update |= dateRow("shipping", "Versanddatum", order.shippingDate, today);

  if (!order.cancelledNote.empty())
    ImGui::TextWrapped("Grund der Stornierung: %s", order.cancelledNote.c_str());

  if (!isNew && !cancelled && order.finishedDate.empty()) {
    const char *label = nullptr;
    if (std::string *next = nextStep(order, &label)) {
      if (ImGui::Button((std::string("Nächster Schritt: ") + label).c_str())) {
        *next = today;
        update = true;
      }
      ImGui::SetItemTooltip("Trägt das heutige Datum beim nächsten offenen Schritt ein");
      ImGui::SameLine();
    }
    {
      ui::Disabled d(order.shippingDate.empty());
      if (ImGui::Button("Abschließen")) {
        order.finishedDate = today;
        update = true;
      }
    }
    if (order.shippingDate.empty())
      ImGui::SetItemTooltip("Versanddatum muss ausgefüllt sein");
    ImGui::SameLine();
    if (ImGui::Button("Stornieren")) {
      mCancelId = order.id;
      mCancelNote.clear();
      ImGui::OpenPopup("Bestellung stornieren");
    }
  }
  if (!order.finishedDate.empty())
    ImGui::Text("Abgeschlossen am: %s", order.finishedDate.c_str());

  if (ImGui::BeginPopupModal("Bestellung stornieren", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted("Grund");
    ui::inputMultiline("##grund", mCancelNote, 3.0f);
    if (ImGui::Button("Stornieren") && mCancelId == order.id) {
      order.cancelledDate = today;
      order.cancelledNote = mCancelNote;
      update = true;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Verwerfen") || ImGui::IsKeyPressed(ImGuiKey_Escape))
      ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }

  if (update && !isNew)
    save(ctx, order);
}

Order &OrdersModule::draftOf(const Order &stored) {
  // ImGui text fields need a buffer that lives across frames; the draft is
  // refreshed from the store whenever nothing is being edited.
  auto [it, inserted] = mDrafts.try_emplace(stored.id, stored);
  if (!inserted && it->second.changeTime != stored.changeTime && !ImGui::IsAnyItemActive())
    it->second = stored;
  return it->second;
}

void OrdersModule::drawOrderList(AppContext &ctx, const std::vector<std::uint64_t> &ids) {
  const bool striped = !ctx.prefs().ordersAsPopup; // popup mode: compact list without stripes
  for (std::size_t i = 0; i < ids.size(); ++i) {
    std::optional<ui::StripedRow> row;
    if (striped)
      row.emplace(i);
    drawOrderEntry(ctx, ids[i]);
  }
}

void OrdersModule::drawOrderEntry(AppContext &ctx, std::uint64_t id) {
  const Order *stored = mStore.find(id);
  if (!stored)
    return;
  ui::ScopedId scope(static_cast<int>(id % 0x7fffffff));
  const std::string label = stored->orderNumber + " -> " + stored->customer + (stored->express ? "  [Express]" : "");
  if (ctx.prefs().ordersAsPopup) {
    if (ImGui::Selectable(label.c_str())) {
      mPopupId = id;
      ImGui::OpenPopup("Bestellung bearbeiten");
    }
    if (mPopupId == id && ImGui::BeginPopupModal("Bestellung bearbeiten", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      drawOrder(ctx, draftOf(*stored), false);
      ImGui::Separator();
      if (ImGui::Button("Schließen") || ImGui::IsKeyPressed(ImGuiKey_Escape))
        ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
    }
  } else if (ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth)) {
    drawOrder(ctx, draftOf(*stored), false);
    ImGui::TreePop();
  }
}

void OrdersModule::drawNewOrderPopup(AppContext &ctx) {
  if (!ImGui::BeginPopupModal("Neue Bestellung", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    return;
  drawOrder(ctx, mNewOrder, true);
  const auto missing = orders::missingFields(mNewOrder);
  if (!missing.empty())
    ui::errorText(util::join(missing, ", ") + " muss angegeben werden");
  {
    ui::Disabled d(!missing.empty());
    if (ImGui::Button("Erstellen")) {
      save(ctx, mNewOrder);
      mNewOrder = {};
      ImGui::CloseCurrentPopup();
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Verwerfen") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    mNewOrder = {};
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndPopup();
}

void OrdersModule::drawExportPopup(AppContext &ctx) {
  if (!ImGui::BeginPopupModal("Excel Export", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    return;
  if (ImGui::Button("Speichern unter...")) {
    mExportFile = platform::saveFileDialog({"Excel:xlsx"});
    if (!mExportFile.empty() && !mExportFile.ends_with(".xlsx"))
      mExportFile += ".xlsx";
  }
  ImGui::TextUnformatted(mExportFile.empty() ? "(keine Datei gewählt)" : mExportFile.c_str());
  {
    ui::Disabled d(mExportFile.empty());
    if (ImGui::Button("Erstellen")) {
      std::vector<Order> list;
      for (const auto &[id, o] : mStore.orders())
        list.push_back(o);
      const auto file = util::pathFromUtf8(mExportFile);
      if (ctx.notify.check(exporting::writeXlsx(file, exporting::buildOrderReport(list)), "Excel Export")) {
        platform::openPath(file);
        ImGui::CloseCurrentPopup();
      }
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Zurück") || ImGui::IsKeyPressed(ImGuiKey_Escape))
    ImGui::CloseCurrentPopup();
  ImGui::EndPopup();
}

void OrdersModule::drawWindow(AppContext &ctx) {
  if (ctx.hasAccess(users::groups::Techsupport)) {
    if (ImGui::Button("Bestellung anlegen")) {
      mNewOrder = {};
      mNewOrder.entryDate = ctx.session().date();
      ImGui::OpenPopup("Neue Bestellung");
    }
    ImGui::SameLine();
  }
  if (ImGui::Button("Excel Export")) {
    mExportFile.clear();
    ImGui::OpenPopup("Excel Export");
  }
  ImGui::SameLine();
  if (ImGui::Checkbox("Als Popup öffnen", &ctx.prefs().ordersAsPopup))
    ctx.savePrefs();
  drawNewOrderPopup(ctx);
  drawExportPopup(ctx);

  if (ui::searchField("ordersearch", mSearch, "Kunde, Bestell-/Rechnungsnummer... (Strg+F)"))
    mSearchResult = mStore.search(mSearch);
  if (!mSearch.empty()) {
    ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);
    if (ImGui::TreeNode("##results", "Ergebnisse (%zu)", mSearchResult.size())) {
      drawOrderList(ctx, mSearchResult);
      ImGui::TreePop();
    }
  }

  ImGui::SeparatorText("Bestellungen");
  for (const auto state : orders::kAllStates) {
    const auto ids = mStore.idsInState(state);
    if (ids.empty())
      continue;
    ui::ScopedId scope(static_cast<int>(state));
    ImGui::PushStyleColor(ImGuiCol_Text, stateColor(state));
    const bool open = ImGui::TreeNode("##state", "%s (%zu)", std::string(orders::stateLabel(state)).c_str(), ids.size());
    ImGui::PopStyleColor();
    if (open) {
      drawOrderList(ctx, ids);
      ImGui::TreePop();
    }
  }
}

} // namespace st::app
