#include "st/service/legacy_import.h"

#include "st/util/strings.h"
#include "st/util/time.h"

#include <cstdlib>
#include <map>

namespace st::service {

using util::json::Json;

std::vector<std::vector<std::string>> parseCsv(std::string_view content, char delimiter) {
  std::vector<std::vector<std::string>> records;
  std::vector<std::string> fields;
  std::string field;
  bool inQuotes = false;
  bool any = false;
  if (content.starts_with("\xEF\xBB\xBF"))
    content.remove_prefix(3);
  for (std::size_t i = 0; i < content.size(); ++i) {
    const char c = content[i];
    any = true;
    if (inQuotes) {
      if (c == '"') {
        if (i + 1 < content.size() && content[i + 1] == '"') {
          field += '"';
          ++i;
        } else {
          inQuotes = false;
        }
      } else {
        field += c;
      }
    } else if (c == '"') {
      inQuotes = true;
    } else if (c == delimiter) {
      fields.push_back(std::move(field));
      field.clear();
    } else if (c == '\n' || c == '\r') {
      if (c == '\r' && i + 1 < content.size() && content[i + 1] == '\n')
        ++i;
      fields.push_back(std::move(field));
      field.clear();
      records.push_back(std::move(fields));
      fields.clear();
      any = false;
    } else {
      field += c;
    }
  }
  if (any) {
    fields.push_back(std::move(field));
    records.push_back(std::move(fields));
  }
  return records;
}

std::string closestServiceFolder(const devices::Device &device, std::string_view date) {
  std::string best;
  long bestDiff = 0;
  for (const auto &dir : util::listDirectory(device.path, true, false)) {
    const std::string name = util::pathToUtf8(dir.filename());
    if (!name.starts_with("Service_"))
      continue;
    const auto diff = util::daysBetween(date, util::splitAt(name, "_").second);
    if (!diff)
      continue;
    const long d = std::labs(*diff);
    if (best.empty() || d < bestDiff) {
      best = name;
      bestDiff = d;
    }
  }
  return best;
}

Json convertLegacyServiceList(std::string_view csvContent, const devices::DeviceRegistry &registry) {
  Json out = Json::array();
  std::map<std::string, std::size_t> indexOf;
  auto records = parseCsv(csvContent);
  if (!records.empty())
    records.erase(records.begin()); // header
  const auto get = [](const std::vector<std::string> &f, std::size_t i) { return i < f.size() ? util::trim(f[i]) : std::string(); };

  for (const auto &f : records) {
    std::string deviceType = get(f, 1);
    const std::string serial = get(f, 2);
    const std::string state = get(f, 13);
    if (deviceType.empty() || serial.empty() || state != "erledigt")
      continue;
    if (deviceType.front() != '0')
      deviceType = "0" + deviceType;
    const std::string name = deviceType + "-" + serial;
    const devices::Device *device = registry.find(name);
    if (!device)
      continue;

    std::string amount = get(f, 7);
    for (const char *token : {"€", ".", " "})
      util::replaceAll(amount, token, "");
    util::replaceAll(amount, ",", ".");
    Json entry;
    entry["total_price"] = util::parseDouble(amount).value_or(0.0);
    entry["end_date"] = util::replaced(get(f, 10), "-", ".");
    entry["name"] = name;
    entry["path"] = util::pathToUtf8(device->path);
    entry["status"] = state;
    entry["activeservice"] = closestServiceFolder(*device, get(f, 0));
    entry["syncfolder"] = "";
    entry["services"] = Json::array();
    const auto orNa = [](const std::string &s) { return s.empty() ? std::string("n/a") : s; };
    entry["additionalinfo"] = "Kunde: " + get(f, 3) + "\nAnsprechpartner: " + get(f, 4) + "\nGrund: " + get(f, 5) +
                              "\nAngebot: " + orNa(get(f, 6)) + "\nAuftrag: " + orNa(get(f, 8));
    entry["duration"] = util::parseInt64(get(f, 12)).value_or(0);

    auto it = indexOf.find(name);
    if (it == indexOf.end()) {
      out.push_back({{"name", name}, {"services", Json::array()}});
      it = indexOf.emplace(name, out.size() - 1).first;
    }
    out[it->second]["services"].push_back(std::move(entry));
  }
  return out;
}

} // namespace st::service
