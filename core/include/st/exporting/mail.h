#pragma once

#include "st/util/fs.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// "E-Mail erstellen": e-mail template and the draft built from it.
///
/// The template lives in configs/mailTemplate.json (shared, edited by power users):
///   {"to": "...", "cc": "...", "subject": "Service Report {reader}", "body": "..."}
/// Placeholders: {reader} {project} {service} {date} {user}. Without the file the
/// default template applies.
namespace st::exporting {

struct MailTemplate {
  /// Recipients, separated by ';'.
  std::string to;
  std::string cc;
  std::string subject;
  std::string body;

  bool operator==(const MailTemplate &) const = default;
};

/// Template used while no mailTemplate.json exists.
MailTemplate defaultMailTemplate();

/// Placeholders offered in the editor with their meaning.
const std::vector<std::pair<std::string, std::string>> &mailPlaceholders();

/// Replaces every "{key}" of `values`; unknown placeholders stay as they are.
std::string expandPlaceholders(std::string_view text, const std::map<std::string, std::string> &values);

struct MailDraft {
  std::string to;
  std::string cc;
  std::string subject;
  std::string body;
  std::vector<util::fs::path> attachments;
};

/// Draft with all placeholders replaced (attachments are added by the caller).
MailDraft buildMailDraft(const MailTemplate &mail, const std::map<std::string, std::string> &values);

/// "mailto:" URL of the draft (fallback without attachments, they cannot be passed
/// this way).
std::string mailtoUrl(const MailDraft &draft);

/// Sub folder of a service folder that holds the reader evaluation.
inline constexpr std::string_view kEvaluationFolder = "EV-Plate";
/// Text that must be part of the evaluation file name.
inline constexpr std::string_view kEvaluationName = "Reader Evaluation";

/// The newest PDF in `<serviceFolder>/EV-Plate` whose name contains "Reader Evaluation"
/// (case-insensitive). Empty if there is none.
std::optional<util::fs::path> findReaderEvaluation(const util::fs::path &serviceFolder);

/// Shared store of the template (see namespace documentation).
class MailTemplateStore {
public:
  MailTemplateStore() = default;
  explicit MailTemplateStore(util::fs::path file) : mFile(std::move(file)) {}

  const util::fs::path &file() const { return mFile; }
  /// Reads the template; a missing file results in the default template.
  util::Status load();
  util::Status save(const MailTemplate &mail);
  const MailTemplate &mail() const { return mMail; }
  /// True if the file changed on disk since the last load/save.
  bool changedOnDisk() const { return util::lastWriteTime(mFile) != mLastWrite; }

private:
  util::fs::path mFile;
  MailTemplate mMail = defaultMailTemplate();
  std::int64_t mLastWrite = 0;
};

} // namespace st::exporting
