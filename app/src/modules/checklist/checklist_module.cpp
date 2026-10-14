#include "modules/checklist/checklist_module.h"

#include "framework/events.h"
#include "framework/platform.h"
#include "framework/widgets.h"
#include "modules/service/service_module.h"

#include <st/checklist/actions.h>
#include <st/checklist/export.h>
#include <st/util/log.h>
#include <st/util/strings.h>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>

namespace st::app {

namespace {
constexpr const char *kWorkWindow = "Checkliste";
constexpr const char *kEditorWindow = "Checklisten Editor";
constexpr const char *kChecklistFilter = "Checkliste:json,txt";
} // namespace

void ChecklistModule::init(AppContext &ctx) {
  mRefresh.setInterval(std::chrono::milliseconds(std::max(250, ctx.settings().refreshIntervalMs)));
  ctx.ui.addWindow(kWorkWindow, [this, &ctx] { drawWorkWindow(ctx); }, true, ImGuiWindowFlags_HorizontalScrollbar);
  if (ctx.isPoweruser())
    ctx.ui.addWindow(kEditorWindow, [this, &ctx] { drawEditorWindow(ctx); }, true, ImGuiWindowFlags_HorizontalScrollbar);
  ctx.events.subscribe<events::ServiceSelected>([this, &ctx](const auto &) { open(ctx); });
  ctx.events.subscribe<events::SaveChecklistRequest>([this, &ctx](const auto &) { saveNow(ctx); });
}

void ChecklistModule::open(AppContext &ctx) {
  auto &session = ctx.checklist();
  if (session.doc && session.dirty)
    saveNow(ctx);
  session.doc.reset();
  session.dirty = false;
  session.lastError.clear();
  mWorkState = {};
  mRecordSelection.clear();

  auto *device = ctx.activeDevice();
  const std::string &service = ctx.selectedService();
  if (device && !service.empty()) {
    if (const auto path = ctx.services().findChecklist(*device, service, ctx.activeRecord())) {
      auto doc = checklist::ChecklistDocument::load(*path);
      if (doc) {
        session.doc = std::move(*doc);
        ctx.events.publish(events::ChecklistOpened{util::pathToUtf8(*path)});
      } else {
        session.lastError = doc.error().message;
        ctx.notify.error(doc.error().message);
      }
    } else {
      session.lastError = "In " + service + " wurde keine Checkliste gefunden.";
    }
  }
  if (!session.doc)
    ctx.events.publish(events::ChecklistClosed{});
  updateAttention(ctx);
}

bool ChecklistModule::saveNow(AppContext &ctx) {
  auto &session = ctx.checklist();
  if (!session.doc)
    return false;
  auto result = session.doc->save();
  if (!result) {
    if (session.lastError != result.error().message)
      ctx.notify.error("Checkliste konnte nicht gespeichert werden: " + result.error().message);
    session.lastError = result.error().message;
    return false;
  }
  session.dirty = false;
  session.lastError.clear();
  updateAttention(ctx);
  ctx.events.publish(events::ChecklistSaved{util::pathToUtf8(session.doc->path())});
  return true;
}

void ChecklistModule::update(AppContext &ctx) {
  auto &session = ctx.checklist();
  if (!session.doc)
    return;
  const auto delay = std::chrono::milliseconds(ctx.settings().autosaveDelayMs);
  if (session.dirty && delay.count() > 0 && std::chrono::steady_clock::now() - session.lastEdit > delay)
    saveNow(ctx);
  if (mRefresh.due()) {
    auto merged = session.doc->refresh();
    if (!merged && session.lastError != merged.error().message) {
      session.lastError = merged.error().message;
      log::warning("Checkliste konnte nicht synchronisiert werden: {}", merged.error().message);
    }
  }
}

void ChecklistModule::shutdown(AppContext &ctx) {
  if (ctx.checklist().dirty)
    saveNow(ctx);
  if (mTemplate && mTemplateDirty)
    ctx.notify.check(mTemplate->save(), "Vorlage speichern");
}

void ChecklistModule::updateAttention(AppContext &ctx) {
  ctx.ui.setAttention(kWorkWindow, ctx.checklist().dirty);
  if (ctx.isPoweruser())
    ctx.ui.setAttention(kEditorWindow, mTemplate ? mTemplateDirty : ctx.checklist().dirty);
}

void ChecklistModule::drawToolbar(AppContext &ctx, checklist::ChecklistDocument &doc, bool editor) {
  auto &session = ctx.checklist();
  const bool isTemplate = editor && mTemplate.has_value();
  const bool dirty = isTemplate ? mTemplateDirty : session.dirty;

  if (ImGui::SmallButton(util::pathToUtf8(doc.path().filename()).c_str()))
    platform::revealInExplorer(doc.path());
  ImGui::SetItemTooltip("%s\nKlicken zeigt die Datei im Explorer", util::pathToUtf8(doc.path()).c_str());
  ImGui::SameLine();
  ImGui::TextDisabled("Version %s", doc.data().version.c_str());
  ImGui::SameLine();
  if (dirty)
    ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.3f, 1.0f), "Ungespeichert");
  else
    ImGui::TextColored(ImVec4(0.45f, 0.8f, 0.45f, 1.0f), "Gespeichert");
  ImGui::SameLine();
  if (ImGui::Button("Speichern") || ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteFocused)) {
    if (isTemplate) {
      if (ctx.notify.check(mTemplate->save(), "Vorlage speichern")) {
        mTemplateDirty = false;
        ctx.notify.success("Vorlage gespeichert");
      }
    } else if (saveNow(ctx)) {
      ctx.notify.success("Checkliste gespeichert");
    }
    updateAttention(ctx);
  }
  ImGui::SetItemTooltip("Strg+S. Änderungen werden außerdem automatisch gespeichert.");

  if (!editor) {
    ImGui::SameLine();
    if (ImGui::Button("Record erstellen"))
      ImGui::OpenPopup("Record erstellen");
    ImGui::SameLine();
    if (ImGui::SmallButton("Alle auf"))
      mWorkState.expandAll = true;
    ImGui::SameLine();
    if (ImGui::SmallButton("Alle zu"))
      mWorkState.expandAll = false;
    ImGui::SameLine();
    ImGui::Checkbox("Nur offene Aufgaben", &mWorkState.onlyOpen);
  }

  const auto [done, total] = ChecklistRenderer::progress(doc.data());
  const float fraction = total > 0 ? static_cast<float>(done) / static_cast<float>(total) : 0.0f;
  const std::string overlay = std::to_string(done) + " / " + std::to_string(total) + " Aufgaben erledigt";
  ImGui::ProgressBar(fraction, ImVec2(-FLT_MIN, 0.0f), overlay.c_str());

  if (doc.data().finished)
    ImGui::TextColored(ImVec4(0.45f, 0.8f, 0.45f, 1.0f), "Abgeschlossen von %s am %s", doc.data().user.c_str(),
                       ctx.displayTime(doc.data().timestamp).c_str());
  if (!session.lastError.empty() && !isTemplate)
    ui::errorText(session.lastError);
}

void ChecklistModule::drawWorkWindow(AppContext &ctx) {
  auto &session = ctx.checklist();
  if (!session.doc) {
    if (!ctx.activeDevice())
      ui::hintText("Gerät in \"Reader Control\" wählen.");
    else if (ctx.selectedService().empty())
      ui::hintText("Service in \"Service Control\" wählen.");
    else
      ui::hintText(session.lastError.empty() ? "Keine Checkliste geöffnet." : session.lastError);
    return;
  }
  drawToolbar(ctx, *session.doc, false);
  ImGui::Separator();
  ChecklistRenderer renderer(ctx, *session.doc, mWorkState, false, [this, &ctx](bool immediate) {
    ctx.checklist().markDirty();
    updateAttention(ctx);
    if (immediate)
      saveNow(ctx);
  });
  renderer.drawSections();
  drawRecordPopup(ctx);
}

void ChecklistModule::drawRecordPopup(AppContext &ctx) {
  auto &session = ctx.checklist();
  if (!session.doc)
    return;
  if (!ImGui::BeginPopupModal("Record erstellen", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    return;
  // keep the chosen order, add newly finished sections at the end
  for (const auto &section : session.doc->data().sections) {
    if (!section.finished)
      continue;
    const bool known = std::any_of(mRecordSelection.begin(), mRecordSelection.end(), [&](const auto &p) { return p.first == section.name; });
    if (!known)
      mRecordSelection.emplace_back(section.name, true);
  }
  ImGui::TextUnformatted("Welche Arbeitsgänge sollen ausgedruckt werden?");
  if (ImGui::Button("Alle"))
    for (auto &[name, active] : mRecordSelection)
      active = true;
  ImGui::SameLine();
  if (ImGui::Button("Keine"))
    for (auto &[name, active] : mRecordSelection)
      active = false;
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
  ImGui::Separator();
  if (ImGui::Button("Record erstellen")) {
    std::vector<std::string> names;
    for (const auto &[name, active] : mRecordSelection)
      if (active)
        names.push_back(name);
    auto options = ServiceModule::htmlOptions(ctx);
    const auto stem = util::pathToUtf8(session.doc->path().stem());
    options.documentName = stem + "_ServiceRecord.html";
    const auto file = util::fs::temp_directory_path() / "ServiceTracker" / util::pathFromUtf8(options.documentName);
    const auto html = checklist::exportHtml(checklist::recordSubset(session.doc->data(), names), options);
    if (ctx.notify.check(util::writeFileAtomic(file, html), "Record erstellen"))
      platform::openPath(file);
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (ImGui::Button("Abbrechen") || ImGui::IsKeyPressed(ImGuiKey_Escape))
    ImGui::CloseCurrentPopup();
  ImGui::EndPopup();
}

void ChecklistModule::drawEditorWindow(AppContext &ctx) {
  auto &session = ctx.checklist();

  // --- target selection ---
  if (ImGui::Button("Vorlage laden...")) {
    const auto path = platform::openFileDialog({kChecklistFilter});
    if (!path.empty()) {
      auto doc = checklist::ChecklistDocument::load(util::pathFromUtf8(path));
      if (ctx.notify.check(doc, "Vorlage laden")) {
        mTemplate = std::move(*doc);
        mTemplateDirty = false;
        mEditorState = {};
      }
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Neue Vorlage...")) {
    const auto path = platform::saveFileDialog({"Checkliste:json", "Checkliste (alt):txt"});
    if (!path.empty()) {
      checklist::ChecklistDocument doc(util::pathFromUtf8(path), {});
      doc.data().version = "1.0";
      if (ctx.notify.check(doc.saveAs(doc.path()), "Neue Vorlage")) {
        mTemplate = std::move(doc);
        mTemplateDirty = false;
        mEditorState = {};
      }
    }
  }
  if (mTemplate) {
    ImGui::SameLine();
    if (ImGui::Button("Aktive Checkliste bearbeiten")) {
      if (mTemplateDirty)
        ctx.notify.check(mTemplate->save(), "Vorlage speichern");
      mTemplate.reset();
      mTemplateDirty = false;
      mEditorState = {};
    }
  }

  checklist::ChecklistDocument *doc = mTemplate ? &*mTemplate : (session.doc ? &*session.doc : nullptr);
  if (!doc) {
    ui::hintText("Keine Checkliste geöffnet. Service wählen oder eine Vorlage laden.");
    return;
  }
  ImGui::SameLine();
  if (ImGui::Button("Speichern als...")) {
    const auto path = platform::saveFileDialog({"Checkliste:json", "Checkliste (alt):txt", "CSV:csv", "HTML:html"});
    if (!path.empty()) {
      const auto target = util::pathFromUtf8(path);
      const auto ext = util::pathToUtf8(target.extension());
      util::Status status;
      if (ext == ".csv")
        status = util::writeFileAtomic(target, checklist::exportCsv(doc->data(), util::pathToUtf8(doc->path())));
      else if (ext == ".html")
        status = util::writeFileAtomic(target, checklist::exportHtml(doc->data(), ServiceModule::htmlOptions(ctx)));
      else
        status = doc->saveAs(target);
      if (ctx.notify.check(status, "Speichern als"))
        ctx.notify.success("Gespeichert: " + path);
    }
  }

  ImGui::TextColored(mTemplate ? ImVec4(0.6f, 0.75f, 1.0f, 1.0f) : ImVec4(0.95f, 0.75f, 0.3f, 1.0f), "%s",
                     mTemplate ? "Bearbeitet eine Vorlage" : "Bearbeitet die Checkliste des aktiven Service");
  drawToolbar(ctx, *doc, true);

  const auto onChange = [this, &ctx, isTemplate = mTemplate.has_value()](bool immediate) {
    if (isTemplate) {
      mTemplateDirty = true;
    } else {
      ctx.checklist().markDirty();
      if (immediate)
        saveNow(ctx);
    }
    updateAttention(ctx);
  };

  auto &data = doc->data();
  ImGui::SetNextItemWidth(150.0f);
  if (ImGui::InputText("Version", &data.version))
    onChange(false);
  ImGui::SameLine();
  bool finished = data.finished;
  if (ImGui::Checkbox("Checkliste fertig", &finished)) {
    if (finished) {
      checklist::finishChecklist(data, ctx.expressions, ctx.sectionActions, ctx.session());
      onChange(true);
      const auto base = doc->path().parent_path() / doc->path().stem();
      auto csv = base;
      csv += ".csv";
      auto html = base;
      html += ".html";
      auto options = ServiceModule::htmlOptions(ctx);
      options.documentName = util::pathToUtf8(html.filename());
      ctx.notify.check(util::writeFileAtomic(csv, checklist::exportCsv(data, util::pathToUtf8(doc->path()))), "CSV-Export");
      if (ctx.notify.check(util::writeFileAtomic(html, checklist::exportHtml(data, options)), "HTML-Export"))
        platform::openPath(html);
    } else {
      data.finished = false;
      data.user = ctx.user();
      data.timestamp = ctx.session().timestamp();
      onChange(true);
    }
  }

  ImGui::SeparatorText("Neuer Arbeitsgang");
  ImGui::SetNextItemWidth(250.0f);
  const bool enter = ImGui::InputTextWithHint("##newsection", "Name des Arbeitsgangs", &mNewSectionName, ImGuiInputTextFlags_EnterReturnsTrue);
  ImGui::SameLine();
  const bool exists = data.findSection(mNewSectionName) != nullptr;
  {
    ui::Disabled d(util::trim(mNewSectionName).empty() || exists);
    if (ImGui::Button("Arbeitsgang hinzufügen") || (enter && !exists && !util::trim(mNewSectionName).empty())) {
      checklist::Section section;
      section.name = util::trim(mNewSectionName);
      data.sections.push_back(std::move(section));
      mNewSectionName.clear();
      onChange(false);
    }
  }
  if (exists)
    ui::errorText("Ein Arbeitsgang mit diesem Namen existiert bereits.");
  ImGui::Separator();

  ChecklistRenderer renderer(ctx, *doc, mEditorState, true, onChange);
  renderer.drawSections();
}

} // namespace st::app
