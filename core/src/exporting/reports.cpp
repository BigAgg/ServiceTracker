#include "st/exporting/reports.h"

#include "st/devices/device.h"
#include "st/util/time.h"

#include <cmath>
#include <map>

namespace st::exporting {

namespace {

struct OrderFigures {
  long long total = 0;
  long long open = 0;
  long long finished = 0;
  long long shipped = 0;
  long long cancelled = 0;
  double revenue = 0.0;
  long long processingDays = 0;
  long long shippingDays = 0;

  void add(const orders::Order &o) {
    ++total;
    if (!o.cancelledDate.empty()) {
      ++cancelled;
      return;
    }
    revenue += o.price;
    if (!o.finishedDate.empty()) {
      ++finished;
      processingDays += o.processingDays();
    } else {
      ++open;
    }
    if (!o.shippingDate.empty() && !o.preparedDate.empty()) {
      ++shipped;
      if (const auto d = util::daysBetween(o.preparedDate, o.shippingDate))
        shippingDays += std::abs(*d);
    }
  }

  std::vector<Row> rows() const {
    const auto avg = [](long long sum, long long count) { return count > 0 ? static_cast<double>(sum) / static_cast<double>(count) : 0.0; };
    return {
        {std::string("Bestellungen"), total},
        {std::string("Offen"), open},
        {std::string("Abgeschlossen"), finished},
        {std::string("Verschickt"), shipped},
        {std::string("Storniert"), cancelled},
        {std::string("Gesamtumsatz"), revenue, std::string("€")},
        {std::string("Durchschnittliche Bearbeitungszeit"), avg(processingDays, finished), std::string("Tage")},
        {std::string("Insgesamte Bearbeitungszeit"), processingDays, std::string("Tage")},
        {std::string("Durchschnittliche Zeit im Versand"), avg(shippingDays, shipped), std::string("Tage")},
        {std::string("Gesamtzeit im Versand"), shippingDays, std::string("Tage")},
    };
  }
};

Row orderRow(const orders::Order &o) {
  return {std::to_string(o.id), o.customer, o.orderNumber, o.billNumber, static_cast<long long>(o.processingDays()), o.price,
          o.entryDate, o.bookedDate, o.preparedDate, o.shippingDate, o.finishedDate, std::string(orders::stateLabel(o.state())),
          o.cancelledDate, o.cancelledNote};
}

const Row kOrderHeader = {std::string("ID"), std::string("Kunde"), std::string("Bestellnummer"), std::string("Rechnungsnummer"),
                          std::string("Durchlaufzeit"), std::string("Preis"), std::string("Eingegangen am"), std::string("Sivas Gebucht am"),
                          std::string("Versandbereit am"), std::string("Versendet am"), std::string("Abgeschlossen am"), std::string("Status"),
                          std::string("Storniert am"), std::string("Grund")};

Sheet orderSheet(const std::string &name, const std::vector<const orders::Order *> &list) {
  OrderFigures figures;
  for (const auto *o : list)
    figures.add(*o);
  Sheet sheet;
  sheet.name = name;
  sheet.rows = figures.rows();
  sheet.rows.push_back({});
  sheet.boldRows.push_back(sheet.rows.size());
  sheet.rows.push_back(kOrderHeader);
  for (const auto *o : list)
    sheet.rows.push_back(orderRow(*o));
  return sheet;
}

Sheet historySheet(const std::string &name, const std::vector<service::HistoryEntry> &rows, std::string_view project) {
  const auto stats = service::computeStatistics(rows, project);
  Sheet sheet;
  sheet.name = name;
  sheet.rows = {
      {std::string("Services insgesamt"), static_cast<long long>(stats.services)},
      {std::string("Upgrades"), static_cast<long long>(stats.upgrades)},
      {std::string("Reparaturen"), static_cast<long long>(stats.repairs)},
      {std::string("Garantie / Kulanz"), static_cast<long long>(stats.goodwill)},
      {std::string("Durchschnittliche Durchlaufzeit"), stats.averageDuration, std::string("Tage")},
      {std::string("Umsatz"), stats.revenue, std::string("€")},
      {},
  };
  sheet.boldRows.push_back(sheet.rows.size());
  sheet.rows.push_back({std::string("Reader"), std::string("Service"), std::string("Info"), std::string("Service Pfad"),
                        std::string("Sync Pfad"), std::string("Preis"), std::string("Durchlaufzeit"), std::string("Abschlussdatum"),
                        std::string("Status")});
  for (const auto &row : rows) {
    if (!project.empty() && devices::projectOf(row.device) != project)
      continue;
    sheet.rows.push_back({row.device, row.record.activeService, row.record.additionalInfo, row.servicePath(), row.record.syncFolder,
                          row.totalPrice, static_cast<long long>(row.record.duration), row.endDate, row.record.status});
  }
  return sheet;
}

} // namespace

std::vector<Sheet> buildOrderReport(const std::vector<orders::Order> &orderList) {
  std::vector<const orders::Order *> all;
  std::map<std::string, std::vector<const orders::Order *>> byCustomer;
  for (const auto &o : orderList) {
    all.push_back(&o);
    byCustomer[o.customer.empty() ? std::string("Ohne Kunde") : o.customer].push_back(&o);
  }
  std::vector<Sheet> sheets;
  sheets.push_back(orderSheet("Bestellungen", all));
  for (const auto &[customer, list] : byCustomer)
    sheets.push_back(orderSheet(customer, list));
  return sheets;
}

std::vector<Sheet> buildHistoryReport(const std::vector<service::HistoryEntry> &rows) {
  std::vector<Sheet> sheets;
  sheets.push_back(historySheet("Jahresinformation", rows, {}));
  for (const auto &project : service::projectsOf(rows))
    sheets.push_back(historySheet(project, rows, project));
  return sheets;
}

} // namespace st::exporting
