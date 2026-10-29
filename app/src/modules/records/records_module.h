#pragma once

#include "framework/app_context.h"
#include "framework/background_job.h"
#include "framework/module.h"

#include <st/exporting/mail.h>
#include <st/exporting/record_export.h>

#include <chrono>
#include <string>
#include <utility>
#include <vector>

namespace st::app {

/// Service records and e-mails of the open checklist (see docs/module/records.md).
///
///  - "Record erstellen" (dialog): chosen sections, optionally translated into English,
///    as PDF (Edge headless) with page header/footer, HTML as fallback.
///  - "E-Mail erstellen": English PDF record of the sections allowed for the e-mail
///    export, without working times and names, plus the reader evaluation from
///    "EV-Plate" as Outlook draft built from the template (configs/mailTemplate.json).
///  - Dialog "E-Mail-Vorlage" (menu "Configs") for power users.
///
/// Translation, PDF printing and Outlook run on a worker thread; the UI stays usable.
class RecordsModule final : public Module {
public:
  std::string_view name() const override { return "records"; }
  void init(AppContext &ctx) override;
  void update(AppContext &ctx) override;
  void drawOverlay(AppContext &ctx) override;
  void shutdown(AppContext &ctx) override;

  /// Starts "Record erstellen" for the open checklist with the given sections (in
  /// this order). The result is opened when it is ready.
  void createRecord(AppContext &ctx, const std::vector<std::string> &sections, bool translate, bool pdf);
  /// Starts "E-Mail erstellen" for the open checklist.
  void createMail(AppContext &ctx);

  /// True while a record or e-mail is being created.
  bool busy() const { return mJob.running(); }
  /// Waits for a running export and processes its result (smoke test).
  void finishPendingJob(AppContext &ctx);

private:
  /// Everything a worker needs – copies only, no AppContext.
  struct JobInput {
    exporting::RecordRequest request;
    config::Settings settings;
    /// E-mail: the draft (without the record) and the reader evaluation, if any.
    bool mail = false;
    exporting::MailDraft draft;
  };
  struct JobOutcome {
    util::Result<exporting::RecordResult> record;
    bool mail = false;
    exporting::MailDraft draft;
    util::Status mailStatus;
  };

  static JobOutcome runJob(JobInput input);
  void startJob(AppContext &ctx, JobInput input);
  void handleOutcome(AppContext &ctx, JobOutcome outcome);

  void drawRecordDialog(AppContext &ctx);
  void drawProgress(AppContext &ctx);
  void showMailEditor();
  void drawMailEditor(AppContext &ctx);

  BackgroundJob<JobOutcome> mJob;
  std::chrono::steady_clock::time_point mJobStart{};
  std::string mJobLabel;

  // "Record erstellen": section name + selected, options
  bool mOpenRecordDialog = false;
  std::vector<std::pair<std::string, bool>> mRecordSelection;
  std::string mSelectionFor;
  bool mTranslate = false;
  bool mPdf = true;

  // e-mail template
  exporting::MailTemplateStore mMailStore;
  PollTimer mMailPoll;
  bool mEditorOpen = false;
  bool mEditorDirty = false;
  exporting::MailTemplate mDraftTemplate;
};

} // namespace st::app
