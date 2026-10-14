#pragma once

#include "st/devices/device_registry.h"
#include "st/util/json.h"

#include <string>
#include <string_view>
#include <vector>

/// Import of the old Excel service list (exported as ';' separated CSV) into the
/// History.json format ("Alte Liste konvertieren").
///
/// Expected columns: Anlieferung; Gerät; Seriennummer; Kunde; Ansprechpartner;
/// Grund/Dienstleistung; Angebotsnr; Betrag; Auftragsnr; zum Versand freigegeben;
/// Versand; Durchlaufzeit Service; Durchlaufzeit Versand; Status; Notizen
namespace st::service {

/// Splits CSV content into records; quoted fields may contain ';', '""' and line breaks.
std::vector<std::vector<std::string>> parseCsv(std::string_view content, char delimiter = ';');

/// Service folder of `device` whose date is closest to `date` ("YYYY-MM-DD" or "YYYY.MM.DD").
std::string closestServiceFolder(const devices::Device &device, std::string_view date);

/// Converts the CSV list. Only finished entries ("erledigt") of known devices are imported.
util::json::Json convertLegacyServiceList(std::string_view csvContent, const devices::DeviceRegistry &registry);

} // namespace st::service
