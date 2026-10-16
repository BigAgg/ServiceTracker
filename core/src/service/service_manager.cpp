#include "st/service/service_manager.h"

#include "st/checklist/document.h"

#include "st/util/log.h"
#include "st/util/strings.h"

#include <algorithm>

namespace st::service {

namespace fs = util::fs;

namespace {
std::string u8(const fs::path &p) { return util::pathToUtf8(p); }

util::Error error(std::string message) { return util::Error{std::move(message)}; }

/// Checklist files (".json"/".txt" containing "Checklist") directly inside `folder`.
std::vector<fs::path> checklistFiles(const fs::path &folder) {
  std::vector<fs::path> out;
  for (const auto &file : util::listDirectory(folder, false, true)) {
    const std::string name = u8(file.filename());
    const std::string ext = util::toLower(u8(file.extension()));
    if (name.find("Checklist") != std::string::npos && (ext == ".json" || ext == ".txt"))
      out.push_back(file);
  }
  return out;
}
} // namespace

ServiceManager::ServiceManager(ServiceEnvironment env) : mEnv(std::move(env)) {}

std::string ServiceManager::serviceFolderName(std::string_view date) { return "Service_" + std::string(date); }

std::string ServiceManager::serviceDate(std::string_view folderName) { return util::splitAt(folderName, "_").second; }

std::vector<std::string> ServiceManager::listServices(const devices::Device &device) const {
  std::vector<std::string> out;
  for (const auto &dir : util::listDirectory(device.path, true, false)) {
    std::string name = u8(dir.filename());
    if (name.starts_with("Service_"))
      out.push_back(std::move(name));
  }
  return out;
}

fs::path ServiceManager::syncFolderFor(const devices::Device &device, std::string_view service) const {
  return mEnv.syncTempDir / util::pathFromUtf8(device.name) / util::pathFromUtf8(service);
}

fs::path ServiceManager::externCopyFor(const devices::Device &device) const {
  return mEnv.externLocalDir / util::pathFromUtf8(device.name);
}

util::Result<ServiceRecord> ServiceManager::recordFor(const devices::Device &device) const {
  auto found = mEnv.syncStore.find(device.name);
  if (!found)
    return std::unexpected(found.error());
  ServiceRecord record = found->value_or(ServiceRecord{});
  record.name = device.name;
  if (record.path.empty() || util::pathExists(device.path))
    record.path = u8(device.path);
  record.services = listServices(device);
  return record;
}

std::optional<fs::path> ServiceManager::findChecklist(const devices::Device &device, std::string_view service, const ServiceRecord &record) const {
  if (service.empty())
    return std::nullopt;
  std::vector<fs::path> folders;
  if (isExternalWorkStatus(record.status) && record.activeService == service)
    folders.push_back(externCopyFor(device) / util::pathFromUtf8(service));
  // same order as the legacy program: device folder first, then the sync folder
  folders.push_back(device.path / util::pathFromUtf8(service));
  folders.push_back(syncFolderFor(device, service));
  for (const auto &folder : folders) {
    std::optional<fs::path> txt;
    for (const auto &file : util::listDirectory(folder, false, true)) {
      const std::string name = u8(file.filename());
      if (name.find("Checklist") == std::string::npos)
        continue;
      const std::string ext = util::toLower(u8(file.extension()));
      if (ext == ".json")
        return file;
      if (ext == ".txt" && !txt)
        txt = file;
    }
    if (txt)
      return txt;
  }
  return std::nullopt;
}

std::optional<fs::path> ServiceManager::mirrorOf(const devices::Device &device, const fs::path &file) const {
  const auto rel = file.lexically_normal().lexically_relative(externCopyFor(device).lexically_normal());
  if (rel.empty() || rel.native().starts_with(fs::path("..").native()))
    return std::nullopt;
  return device.path / rel;
}

util::Status ServiceManager::createServiceFolders(const devices::Device &device, const fs::path &serviceFolder) const {
  if (auto s = util::createDirectories(serviceFolder); !s)
    return s;
  const devices::DeviceConfig *config = mEnv.configs ? mEnv.configs->find(device.project()) : nullptr;
  if (!config)
    return {};
  std::vector<std::string> folders = config->folders;
  for (const auto &rule : config->addresses)
    if (rule.flag != 0 && (device.flags & rule.flag) == rule.flag)
      folders.insert(folders.end(), rule.folders.begin(), rule.folders.end());
  for (const auto &folder : folders)
    if (!folder.empty())
      if (auto s = util::createDirectories(serviceFolder / util::pathFromUtf8(folder)); !s)
        return s;
  return {};
}

util::Status ServiceManager::copyTemplates(const devices::Device &device, const std::string &date, const fs::path &target, bool external) const {
  const devices::DeviceConfig *config = mEnv.configs ? mEnv.configs->find(device.project()) : nullptr;
  if (!config)
    return {};
  if (auto s = util::createDirectories(target); !s)
    return s;
  for (const auto &file : config->templatesFor(external)) {
    const auto name = devices::DeviceConfig::expandName(file.rename, device.name, date);
    if (auto s = util::copyFile(util::pathFromUtf8(file.path), target / util::pathFromUtf8(name)); !s)
      return s;
  }
  return {};
}

util::Result<ServiceRecord> ServiceManager::createService(const devices::Device &device, const util::Session &session) {
  auto record = recordFor(device);
  if (!record)
    return record;
  if (!record->syncFolder.empty())
    return std::unexpected(error("Für " + device.name + " läuft bereits ein Service (" + record->activeService + ")"));
  const std::string date = session.date();
  const std::string folder = serviceFolderName(date);
  const fs::path serviceFolder = device.path / util::pathFromUtf8(folder);
  const fs::path syncFolder = syncFolderFor(device, folder);
  log::info("Service anlegen: {} {} -> {} (Sync: {})", device.name, date, u8(serviceFolder), u8(syncFolder));

  if (auto s = createServiceFolders(device, serviceFolder); !s)
    return std::unexpected(s.error());
  if (auto s = util::createDirectories(syncFolder); !s)
    return std::unexpected(s.error());
  if (auto s = copyTemplates(device, date, syncFolder, false); !s)
    return std::unexpected(s.error());

  record->activeService = folder;
  record->syncFolder = u8(syncFolder);
  record->status = status::Received;
  record->services = listServices(device);
  if (auto s = mEnv.syncStore.upsert(*record); !s)
    return std::unexpected(s.error());
  return record;
}

util::Result<ServiceRecord> ServiceManager::setStatus(const devices::Device &device, std::string_view newStatus) {
  auto record = recordFor(device);
  if (!record)
    return record;
  if (newStatus.empty()) {
    if (auto s = removeFromList(device); !s)
      return std::unexpected(s.error());
    record->status.clear();
    return record;
  }
  record->status = newStatus;
  if (auto s = mEnv.syncStore.upsert(*record); !s)
    return std::unexpected(s.error());
  if (isExternalWorkStatus(newStatus) || newStatus == status::ExternFinished) {
    if (auto s = mEnv.externStore.upsert(*record); !s)
      return std::unexpected(s.error());
  }
  log::info("{}: Status \"{}\"", device.name, newStatus);
  return record;
}

util::Result<ServiceRecord> ServiceManager::expect(const devices::Device &device, const std::string &info) {
  auto record = recordFor(device);
  if (!record)
    return record;
  if (!isClosedStatus(record->status))
    return std::unexpected(error(device.name + " ist bereits in der Liste (Status \"" + record->status + "\")"));
  record->status = status::Expected;
  record->additionalInfo = info;
  record->activeService.clear();
  record->syncFolder.clear();
  if (auto s = mEnv.syncStore.upsert(*record); !s)
    return std::unexpected(s.error());
  return record;
}

util::Result<ServiceRecord> ServiceManager::setAdditionalInfo(const devices::Device &device, const std::string &info) {
  auto record = recordFor(device);
  if (!record)
    return record;
  record->additionalInfo = info;
  if (auto s = mEnv.syncStore.upsert(*record); !s)
    return std::unexpected(s.error());
  if (isExternalWorkStatus(record->status) || record->status == status::ExternFinished)
    if (auto s = mEnv.externStore.upsert(*record); !s)
      return std::unexpected(s.error());
  return record;
}

util::Status ServiceManager::removeFromList(const devices::Device &device) {
  if (auto s = mEnv.syncStore.remove(device.name); !s)
    return s;
  log::info("{} aus der Liste entfernt", device.name);
  return {};
}

double ServiceManager::priceOf(const checklist::Checklist &checklist) const {
  return checklist::tableColumnTotal(checklist, mEnv.priceSection, mEnv.priceTable, mEnv.priceColumn);
}

util::Result<FinishResult> ServiceManager::finalize(const devices::Device &device, ServiceRecord record, const fs::path &workFolder,
                                                    const checklist::Checklist *checklist, const checklist::HtmlOptions &html,
                                                    const HistoryStore &history, const util::Session &session) {
  const std::string service = record.activeService;
  const fs::path serviceFolder = device.path / util::pathFromUtf8(service);
  const std::string today = session.date();
  const std::string reportDate = util::replaced(serviceDate(service), ".", "-");
  FinishResult result;

  // 1. copy the work into the device folder (abort before changing any list on failure)
  if (util::pathExists(workFolder)) {
    if (auto s = util::copyTree(workFolder, serviceFolder); !s)
      return std::unexpected(s.error());
  } else if (auto s = util::createDirectories(serviceFolder); !s) {
    return std::unexpected(s.error());
  }

  // 2. service record (HTML)
  if (checklist) {
    checklist::HtmlOptions options = html;
    const std::string fileName = device.name + "_SR_" + reportDate + ".html";
    if (options.documentName.empty())
      options.documentName = fileName;
    result.reportFile = serviceFolder / util::pathFromUtf8(fileName);
    const std::string content = checklist::exportHtml(*checklist, options);
    if (auto s = util::writeFileAtomic(result.reportFile, content); !s)
      return std::unexpected(s.error());
    if (auto s = util::writeFileAtomic(device.path / util::pathFromUtf8(fileName), content); !s)
      return std::unexpected(s.error());
  }

  // 3. record, info file and history
  if (const auto days = util::daysBetween(serviceDate(service), today))
    record.duration = std::max(0L, *days);
  if (auto s = util::json::save(serviceFolder / util::pathFromUtf8(device.name + "_info.json"), recordToJson(record)); !s)
    return std::unexpected(s.error());
  if (auto s = history.append(record, checklist ? priceOf(*checklist) : 0.0, today); !s)
    return std::unexpected(s.error());

  record.activeService.clear();
  record.syncFolder.clear();
  record.services = listServices(device);
  if (auto s = mEnv.syncStore.upsert(record); !s)
    return std::unexpected(s.error());
  log::info("{}: Service {} abgeschlossen ({} Tage)", device.name, service, record.duration);
  result.record = std::move(record);
  return result;
}

util::Result<FinishResult> ServiceManager::finishService(const devices::Device &device, const checklist::Checklist *checklist,
                                                         const checklist::HtmlOptions &html, const util::Session &session) {
  auto record = recordFor(device);
  if (!record)
    return std::unexpected(record.error());
  if (record->activeService.empty())
    return std::unexpected(error("Für " + device.name + " läuft kein Service"));
  if (isExternalWorkStatus(record->status))
    return std::unexpected(error(device.name + " ist ein externer Service. Bitte zuerst unter \"Externe Services\" \"Extern abschließen\" wählen"));
  if (record->status == status::ExternFinished) {
    record->status = status::DoneExternal;
    auto result = finalize(device, std::move(*record), {}, checklist, html, mEnv.externHistory, session);
    if (!result)
      return result;
    if (auto s = mEnv.externStore.remove(device.name); !s)
      return std::unexpected(s.error());
    return result;
  }
  record->status = status::Done;
  const fs::path work = record->syncFolder.empty() ? fs::path() : util::pathFromUtf8(record->syncFolder);
  return finalize(device, std::move(*record), work, checklist, html, mEnv.history, session);
}

util::Result<ServiceRecord> ServiceManager::createExternService(const devices::Device &device, const std::string &date, const std::string &info,
                                                                const util::Session &session) {
  (void)session;
  if (!util::parseDate(date))
    return std::unexpected(error("Ungültiges Datum: \"" + date + "\""));
  auto record = recordFor(device);
  if (!record)
    return record;
  if (!isClosedStatus(record->status))
    return std::unexpected(error(device.name + " ist bereits in der Liste (Status \"" + record->status + "\")"));
  const std::string folder = serviceFolderName(date);
  const fs::path serviceFolder = device.path / util::pathFromUtf8(folder);
  if (auto s = createServiceFolders(device, serviceFolder); !s)
    return std::unexpected(s.error());
  if (auto s = copyTemplates(device, date, serviceFolder, true); !s)
    return std::unexpected(s.error());
  if (auto s = util::copyTree(device.path, externCopyFor(device)); !s)
    return std::unexpected(s.error());

  record->status = status::PlannedExternal;
  record->activeService = folder;
  record->syncFolder.clear();
  record->additionalInfo = "Geplant für: " + date + "\n" + info;
  record->services = listServices(device);
  if (auto s = mEnv.syncStore.upsert(*record); !s)
    return std::unexpected(s.error());
  if (auto s = mEnv.externStore.upsert(*record); !s)
    return std::unexpected(s.error());
  log::info("Externer Service geplant: {} am {}", device.name, date);
  return record;
}

util::Result<ServiceRecord> ServiceManager::finishExternService(const devices::Device &device) {
  auto record = recordFor(device);
  if (!record)
    return record;
  if (!isExternalWorkStatus(record->status) || record->activeService.empty())
    return std::unexpected(error(device.name + " hat keinen laufenden externen Service"));
  const fs::path localCopy = externCopyFor(device);
  if (!util::isDirectory(device.path))
    return std::unexpected(error("Geräteordner nicht erreichbar: " + u8(device.path)));
  if (!util::isDirectory(localCopy))
    return std::unexpected(error("Lokale Kopie nicht gefunden: " + u8(localCopy)));

  // 1. last checklist sync: changes made on the server meanwhile are not overwritten
  const fs::path localService = localCopy / util::pathFromUtf8(record->activeService);
  for (const auto &file : checklistFiles(localService)) {
    auto doc = checklist::ChecklistDocument::load(file);
    if (!doc)
      return std::unexpected(doc.error());
    if (auto synced = doc->syncWithMirror(*mirrorOf(device, file), localCopy, device.path); !synced)
      return std::unexpected(synced.error());
  }
  // 2. local device folder over the original
  if (auto s = util::copyTree(localCopy, device.path); !s)
    return std::unexpected(s.error());
  // 3. attachment paths of the copied checklists point to the server again
  const fs::path serverService = device.path / util::pathFromUtf8(record->activeService);
  for (const auto &file : checklistFiles(serverService)) {
    auto doc = checklist::ChecklistDocument::load(file);
    if (!doc)
      return std::unexpected(doc.error());
    if (checklist::relocateAttachments(doc->data(), localCopy, device.path) > 0)
      if (auto s = doc->save(); !s)
        return std::unexpected(s.error());
  }

  record->status = status::ExternFinished;
  record->services = listServices(device);
  if (auto s = mEnv.syncStore.upsert(*record); !s)
    return std::unexpected(s.error());
  if (auto s = mEnv.externStore.upsert(*record); !s)
    return std::unexpected(s.error());
  if (auto s = util::removeAll(localCopy); !s)
    log::warning("Lokale Kopie konnte nicht gelöscht werden: {}", s.error().message);
  log::info("Externer Service zurückgeführt: {} {}", device.name, record->activeService);
  return record;
}

util::Status ServiceManager::cancelExternService(const devices::Device &device) {
  if (auto s = mEnv.externStore.remove(device.name); !s)
    return s;
  return removeFromList(device);
}

bool shouldAdvanceStatus(const checklist::Checklist &checklist, const checklist::Section &section, std::string_view currentStatus) {
  if (isExternalWorkStatus(currentStatus) || currentStatus == status::ExternFinished)
    return false;
  int oldIndex = -1;
  int newIndex = -1;
  for (int i = 0; i < static_cast<int>(checklist.sections.size()); ++i) {
    const auto &s = checklist.sections[static_cast<std::size_t>(i)];
    if (&s != &section && s.finished && s.postAction == kUpdateStatusAction && s.postActionArgs == currentStatus)
      oldIndex = i;
    if (s.name == section.name)
      newIndex = i;
  }
  return newIndex > oldIndex;
}

} // namespace st::service
