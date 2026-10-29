#include "modules/records/records_module.h"

#include "framework/events.h"
#include "framework/http.h"
#include "framework/outlook.h"
#include "framework/platform.h"
#include "framework/widgets.h"
#include "modules/service/service_module.h"

#include <st/translate/libretranslate.h>
#include <st/util/log.h>
#include <st/util/strings.h>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>

namespace st::app {

namespace {

constexpr const char *kRecordDialog = "Record erstellen";
constexpr const char *kMailEditor = "E-Mail-Vorlage";

util::fs::path exportDir() { return util::fs::temp_directory_path() / "ServiceTracker"; }

/// File names must not contain characters Windows rejects.
std::string safeFileName(std::string name) {
  for (auto &c : name)
    if (std::string_view("<>:\"/\\|?*").find(c) != std::string_view::npos)
      c = '_';
  return name;
}

} // namespace

void RecordsModule::init(AppContext &ctx) {
  mMailStore = exporting::MailTemplateStore(util::pathFromUtf8(ctx.paths().configsDir) / "mailTemplate.json");
  mMailPoll.setInterval(std::chrono::milliseconds(std::max(1000, ctx.settings().refreshIntervalMs * 5)));
  if (util::isDirectory(mMailStore.file().parent_path()))
    ctx.notify.check(mMailStore.load(), "E-Mail-Vorlage");

  ctx.events.subscribe<events::ShowRecordDialog>([this](const auto &) { mOpenRecordDialog = true; });
  ctx.events.subscribe<events::CreateServiceMail>([this, &ctx](const auto &) { createMail(ctx); });
  ctx.events.subscribe<events::ShowMailTemplateEditor>([this](const auto &) { showMailEditor(); });
}

void RecordsModule::update(AppContext &ctx) {
  if (auto outcome = mJob.take())
    handleOutcome(ctx, std::move(*outcome));
  // template changed by a power user on another PC
  if (!mEditorDirty && mMailPoll.due() && util::isDirectory(mMailStore.file().parent_path()) && mMailStore.changedOnDisk())
    if (ctx.notify.check(mMailStore.load(), "E-Mail-Vorlage") && mEditorOpen)
      mDraftTemplate = mMailStore.mail();
}

void RecordsModule::shutdown(AppContext &ctx) { finishPendingJob(ctx); }

void RecordsModule::finishPendingJob(AppContext &ctx) {
  if (auto outcome = mJob.wait())
    handleOutcome(ctx, std::move(*outcome));
}

RecordsModule::JobOutcome RecordsModule::runJob(JobInput input) {
  const auto &settings = input.settings;
  translate::TranslationCache cache(util::pathFromUtf8(settings.paths.translationsFile));
  if (input.request.translate)
    if (auto s = cache.load(); !s)
      log::warning("Übersetzungsdatei: {}", s.error().message);
  const int timeout = std::max(1000, settings.translatorTimeoutMs);
  translate::LibreTranslateBackend backend(settings.translator, [timeout](const std::string &url, const std::string &body) {
    return http::postJson(url, body, timeout);
  });
  translate::Translator translator(cache, &backend);

  exporting::PdfPrinter printer;
  if (input.request.pdf) {
    const auto browser = platform::findPdfBrowser(settings.pdfBrowser);
    if (!browser.empty())
      printer = [browser](const util::fs::path &html, const util::fs::path &pdf) { return platform::printHtmlToPdf(browser, html, pdf); };
  }

  JobOutcome outcome{exporting::createRecord(std::move(input.request), &translator, printer), input.mail, std::move(input.draft), {}};
  if (outcome.mail && outcome.record) {
    outcome.draft.attachments.insert(outcome.draft.attachments.begin(), outcome.record->file);
    outcome.mailStatus = outlook::createDraft(outcome.draft);
  }
  return outcome;
}

void RecordsModule::startJob(AppContext &ctx, JobInput input) {
  if (mJob.running()) {
    ctx.notify.info("Es wird bereits ein Record erstellt – bitte kurz warten.");
    return;
  }
  (void)util::createDirectories(input.request.outputBase.parent_path());
  mJobLabel = input.mail ? "E-Mail wird vorbereitet" : "Record wird erstellt";
  if (input.request.translate)
    mJobLabel += " (Übersetzung)";
  mJobStart = std::chrono::steady_clock::now();
  mJob.start([input = std::move(input)]() mutable { return runJob(std::move(input)); });
  ctx.events.publish(events::RecordExportState{true});
}

void RecordsModule::handleOutcome(AppContext &ctx, JobOutcome outcome) {
  ctx.events.publish(events::RecordExportState{false});
  if (!ctx.notify.check(outcome.record, outcome.mail ? "E-Mail erstellen" : "Record erstellen"))
    return;
  const auto &record = *outcome.record;
  for (const auto &warning : record.warnings)
    ctx.notify.warning(warning);
  if (record.translatedLines > 0)
    ctx.notify.info(std::to_string(record.translatedLines) + " neue Zeile(n) übersetzt und in der Übersetzungsdatei gespeichert");

  if (!outcome.mail) {
    platform::openPath(record.file);
    return;
  }
  if (outcome.mailStatus) {
    ctx.notify.success("E-Mail in Outlook vorbereitet (" + std::to_string(outcome.draft.attachments.size()) + " Anhang/Anhänge)");
    return;
  }
  // Outlook missing (or the new Outlook): open the mail program without attachments
  // and show the files so they can be dragged into the mail
  ctx.notify.warning("Outlook nicht verfügbar (" + outcome.mailStatus.error().message +
                     "). E-Mail ohne Anhänge geöffnet – die Anhänge sind im Explorer markiert.");
  platform::openUrl(exporting::mailtoUrl(outcome.draft));
  platform::revealInExplorer(record.file);
}

void RecordsModule::createRecord(AppContext &ctx, const std::vector<std::string> &sections, bool translate, bool pdf) {
  auto &session = ctx.checklist();
  if (!session.doc) {
    ctx.notify.warning("Keine Checkliste geöffnet.");
    return;
  }
  JobInput input;
  input.settings = ctx.settings();
  input.request.checklist = checklist::recordSubset(session.doc->data(), sections);
  input.request.html = ServiceModule::htmlOptions(ctx);
  input.request.translate = translate;
  input.request.pdf = pdf;
  const auto stem = util::pathToUtf8(session.doc->path().stem());
  input.request.outputBase = exportDir() / util::pathFromUtf8(safeFileName(stem + "_ServiceRecord" + (translate ? "_EN" : "")));
  startJob(ctx, std::move(input));
}

void RecordsModule::createMail(AppContext &ctx) {
  auto &session = ctx.checklist();
  auto *device = ctx.activeDevice();
  if (!session.doc || !device) {
    ctx.notify.warning("Keine Checkliste geöffnet.");
    return;
  }
  JobInput input;
  input.mail = true;
  input.settings = ctx.settings();
  input.request.checklist = checklist::emailSubset(session.doc->data());
  if (input.request.checklist.sections.empty()) {
    ctx.notify.warning("Kein fertig gemeldeter Arbeitsgang ist für den E-Mail-Export freigegeben.");
    return;
  }
  input.request.html = ServiceModule::htmlOptions(ctx);
  input.request.html.hideWorkInfo = true;
  input.request.translate = true;
  input.request.pdf = true;

  const std::string service = ctx.selectedService();
  const std::string date = service::ServiceManager::serviceDate(service);
  input.request.outputBase = exportDir() / util::pathFromUtf8(safeFileName(device->name + "_Service_Report" + (date.empty() ? "" : "_" + date)));
  input.draft = exporting::buildMailDraft(mMailStore.mail(), {{"reader", device->name},
                                                              {"project", device->project()},
                                                              {"service", service},
                                                              {"date", date},
                                                              {"user", ctx.user()}});
  // reader evaluation: folder of the open checklist first (sync folder, local copy),
  // then the service folder in the device folder
  for (const auto &folder : {session.doc->path().parent_path(), device->path / util::pathFromUtf8(service)}) {
    if (auto evaluation = exporting::findReaderEvaluation(folder)) {
      input.draft.attachments.push_back(*evaluation);
      break;
    }
  }
  if (input.draft.attachments.empty())
    log::info("Keine \"Reader Evaluation\" in {} gefunden", std::string(exporting::kEvaluationFolder));
  startJob(ctx, std::move(input));
}

void RecordsModule::drawOverlay(AppContext &ctx) {
  drawRecordDialog(ctx);
  drawProgress(ctx);
  if (mEditorOpen) {
    ImGui::SetNextWindowSize(ImVec2(640.0f, 520.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(kMailEditor, &mEditorOpen, ImGuiWindowFlags_NoDocking | (mEditorDirty ? ImGuiWindowFlags_UnsavedDocument : 0)))
      drawMailEditor(ctx);
    ImGui::End();
  }
}

void RecordsModule::drawProgress(AppContext &ctx) {
  (void)ctx;
  if (!mJob.running())
    return;
  const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - mJobStart).count();
  const ImGuiViewport *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f, viewport->WorkPos.y + 40.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
  ImGui::SetNextWindowBgAlpha(0.9f);
  const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDocking;
  if (ImGui::Begin("##recordprogress", nullptr, flags)) {
    static constexpr const char *kSpinner = "|/-\\";
    ImGui::Text("%c  %s ... %llds", kSpinner[(seconds) % 4], mJobLabel.c_str(), static_cast<long long>(seconds));
  }
  ImGui::End();
}

void RecordsModule::drawRecordDialog(AppContext &ctx) {
  auto &session = ctx.checklist();
  if (mOpenRecordDialog) {
    mOpenRecordDialog = false;
    if (session.doc)
      ImGui::OpenPopup(kRecordDialog);
  }
  if (!ImGui::BeginPopupModal(kRecordDialog, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    return;
  if (!session.doc) {
    ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
    return;
  }
  // a different checklist starts with a fresh selection
  const std::string current = util::pathToUtf8(session.doc->path());
  if (current != mSelectionFor) {
    mRecordSelection.clear();
    mSelectionFor = current;
  }
  // keep the chosen order, add newly finished sections at the end
  for (const auto &section : session.doc->data().sections) {
    if (!section.finished)
      continue;
    const bool known = std::any_of(mRecordSelection.begin(), mRecordSelection.end(), [&](const auto &p) { return p.first == section.name; });
    if (!known)
      mRecordSelection.emplace_back(section.name, true);
  }
  std::erase_if(mRecordSelection, [&](const auto &p) {
    const auto *section = session.doc->data().findSection(p.first);
    return !section || !section->finished;
  });

  ImGui::TextUnformatted("Welche Arbeitsgänge sollen ausgedruckt werden?");
  if (ImGui::Button("Alle"))
    for (auto &[name, active] : mRecordSelection)
      active = true;
  ImGui::SameLine();
  if (ImGui::Button("Keine"))
    for (auto &[name, active] : mRecordSelection)
      active = false;
  ImGui::SameLine();
  if (ImGui::Button("Wie E-Mail-Export"))
    for (auto &[name, active] : mRecordSelection)
      active = session.doc->data().findSection(name)->emailExport;
  ImGui::SetItemTooltip("Wählt die Arbeitsgänge, die im E-Mail-Export erscheinen dürfen");
  if (mRecordSelection.empty())
    ui::hintText("Es gibt noch keine fertig gemeldeten Arbeitsgänge.");
  for (std::size_t i = 0; i < mRecordSelection.size(); ++i) {
    ui::ScopedId id(static_cast<int>(i));
    {
      ui::Disabled d(i + 1 >= mRecordSelection.size());
      if (ImGui::ArrowButton("##down", ImGuiDir_Down))
        std::swap(mRecordSelection[i], mRecordSelection[i + 1]);
    }
    ImGui::SameLine();
    {
      ui::Disabled d(i == 0);
      if (ImGui::ArrowButton("##up", ImGuiDir_Up))
        std::swap(mRecordSelection[i], mRecordSelection[i - 1]);
    }
    ImGui::SameLine();
    ImGui::Checkbox(mRecordSelection[i].first.c_str(), &mRecordSelection[i].second);
  }

  ImGui::SeparatorText("Optionen");
  ImGui::Checkbox("In englisch übersetzen", &mTranslate);
  ImGui::SetItemTooltip("Übersetzt über LibreTranslate (%s).\nBekannte Sätze kommen aus der Übersetzungsdatei:\n%s",
                        ctx.settings().translator.url.c_str(), ctx.paths().translationsFile.c_str());
  ImGui::Checkbox("Als PDF erstellen", &mPdf);
  ImGui::SetItemTooltip("PDF mit Kopf- und Fußzeile und Seitenzahlen (über Microsoft Edge).\nOhne Haken oder ohne Edge: HTML.");

  ImGui::Separator();
  const bool any = std::any_of(mRecordSelection.begin(), mRecordSelection.end(), [](const auto &p) { return p.second; });
  {
    ui::Disabled d(!any || mJob.running());
    if (ImGui::Button("Record erstellen")) {
      std::vector<std::string> names;
      for (const auto &[name, active] : mRecordSelection)
        if (active)
          names.push_back(name);
      createRecord(ctx, names, mTranslate, mPdf);
      ImGui::CloseCurrentPopup();
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Abbrechen") || ImGui::IsKeyPressed(ImGuiKey_Escape))
    ImGui::CloseCurrentPopup();
  ImGui::EndPopup();
}

void RecordsModule::showMailEditor() {
  mDraftTemplate = mMailStore.mail();
  mEditorDirty = false;
  mEditorOpen = true;
}

void RecordsModule::drawMailEditor(AppContext &ctx) {
  ui::hintText("Vorlage für \"E-Mail erstellen\" in der Checkliste. Datei: " + util::pathToUtf8(mMailStore.file()));
  ImGui::SetNextItemWidth(-FLT_MIN);
  mEditorDirty |= ImGui::InputTextWithHint("##to", "An (mehrere mit ; trennen)", &mDraftTemplate.to);
  ImGui::SetNextItemWidth(-FLT_MIN);
  mEditorDirty |= ImGui::InputTextWithHint("##cc", "CC", &mDraftTemplate.cc);
  ImGui::SetNextItemWidth(-FLT_MIN);
  mEditorDirty |= ImGui::InputTextWithHint("##subject", "Betreff", &mDraftTemplate.subject);
  mEditorDirty |= ui::inputMultiline("##body", mDraftTemplate.body, 10.0f);

  ImGui::SeparatorText("Platzhalter (Klick fügt in den Text ein)");
  for (const auto &[key, description] : exporting::mailPlaceholders()) {
    if (ImGui::SmallButton(key.c_str())) {
      mDraftTemplate.body += key;
      mEditorDirty = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", description.c_str());
  }
  ui::hintText("Anhänge: übersetzter Record (PDF) und die \"Reader Evaluation\" aus dem Ordner EV-Plate des Service.");

  ImGui::Separator();
  {
    ui::Disabled d(!mEditorDirty);
    if (ImGui::Button("Speichern")) {
      if (ctx.notify.check(mMailStore.save(mDraftTemplate), "E-Mail-Vorlage speichern")) {
        mEditorDirty = false;
        ctx.notify.success("E-Mail-Vorlage gespeichert");
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Verwerfen"))
      showMailEditor();
  }
  ImGui::SameLine();
  if (ImGui::Button("Standard wiederherstellen")) {
    const auto defaults = exporting::defaultMailTemplate();
    mDraftTemplate.subject = defaults.subject;
    mDraftTemplate.body = defaults.body;
    mEditorDirty = true;
  }
  ImGui::SetItemTooltip("Setzt Betreff und Text zurück (Empfänger bleiben, erst mit \"Speichern\" wirksam)");
}

} // namespace st::app
