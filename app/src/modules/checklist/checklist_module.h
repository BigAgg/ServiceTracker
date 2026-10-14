#pragma once

#include "framework/app_context.h"
#include "framework/module.h"
#include "modules/checklist/checklist_renderer.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace st::app {

/// Checklist of the selected service.
/// Windows: "Checkliste" (work), "Checklisten Editor" (power users, structure and templates).
///
/// The checklist lives in AppContext::checklist() so other modules (service, extern)
/// can finish and save it. Changes are saved automatically after a short idle time
/// and changes of other users are merged in regularly.
class ChecklistModule final : public Module {
public:
  std::string_view name() const override { return "checklist"; }
  void init(AppContext &ctx) override;
  void update(AppContext &ctx) override;
  void shutdown(AppContext &ctx) override;

private:
  void open(AppContext &ctx);
  bool saveNow(AppContext &ctx);
  void drawWorkWindow(AppContext &ctx);
  void drawEditorWindow(AppContext &ctx);
  void drawToolbar(AppContext &ctx, checklist::ChecklistDocument &doc, bool editor);
  void drawRecordPopup(AppContext &ctx);
  void updateAttention(AppContext &ctx);

  ChecklistViewState mWorkState;
  ChecklistViewState mEditorState;
  PollTimer mRefresh;

  // Editor: optional separate template document
  std::optional<checklist::ChecklistDocument> mTemplate;
  bool mTemplateDirty = false;
  std::string mNewSectionName;

  // "Record erstellen": section name + selected
  std::vector<std::pair<std::string, bool>> mRecordSelection;
};

} // namespace st::app
