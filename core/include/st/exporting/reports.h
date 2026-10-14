#pragma once

#include "st/exporting/xlsx.h"
#include "st/orders/order_store.h"
#include "st/service/history.h"

#include <vector>

/// Builds the Excel reports. Pure functions so the content is unit testable
/// independent of the xlsx writer.
namespace st::exporting {

/// Order export: one sheet "Bestellungen" with all orders plus one sheet per customer,
/// each starting with key figures followed by the order table.
std::vector<Sheet> buildOrderReport(const std::vector<orders::Order> &orders);

/// Yearly service export: one overview sheet plus one sheet per project.
std::vector<Sheet> buildHistoryReport(const std::vector<service::HistoryEntry> &rows);

} // namespace st::exporting
