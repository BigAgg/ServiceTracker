#include "st/exporting/mail.h"

#include "st/util/json.h"
#include "st/util/strings.h"

#include <format>

namespace st::exporting {

namespace {

std::string percentEncode(std::string_view text) {
  std::string out;
  for (const char ch : text) {
    const auto c = static_cast<unsigned char>(ch);
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~')
      out += ch;
    else
      out += std::format("%{:02X}", c);
  }
  return out;
}

} // namespace

MailTemplate defaultMailTemplate() {
  MailTemplate mail;
  mail.subject = "Service Report {reader}";
  mail.body = "Dear Sir or Madam,\n\n"
              "please find attached the service report of reader {reader} (service from {date}).\n\n"
              "Best regards\n{user}";
  return mail;
}

const std::vector<std::pair<std::string, std::string>> &mailPlaceholders() {
  static const std::vector<std::pair<std::string, std::string>> placeholders = {
      {"{reader}", "Gerät, z. B. 0601-1234"},
      {"{project}", "Projektnummer, z. B. 0601"},
      {"{service}", "Service-Ordner, z. B. Service_2026.10.29"},
      {"{date}", "Datum des Service, z. B. 2026.10.29"},
      {"{user}", "angemeldeter Benutzer"},
  };
  return placeholders;
}

std::string expandPlaceholders(std::string_view text, const std::map<std::string, std::string> &values) {
  std::string out(text);
  for (const auto &[key, value] : values)
    util::replaceAll(out, "{" + key + "}", value);
  return out;
}

MailDraft buildMailDraft(const MailTemplate &mail, const std::map<std::string, std::string> &values) {
  MailDraft draft;
  draft.to = util::trim(expandPlaceholders(mail.to, values));
  draft.cc = util::trim(expandPlaceholders(mail.cc, values));
  draft.subject = expandPlaceholders(mail.subject, values);
  draft.body = expandPlaceholders(mail.body, values);
  return draft;
}

std::string mailtoUrl(const MailDraft &draft) {
  std::string url = "mailto:" + percentEncode(draft.to);
  std::vector<std::string> query;
  if (!draft.cc.empty())
    query.push_back("cc=" + percentEncode(draft.cc));
  if (!draft.subject.empty())
    query.push_back("subject=" + percentEncode(draft.subject));
  if (!draft.body.empty()) {
    std::string body = draft.body;
    util::replaceAll(body, "\r\n", "\n");
    util::replaceAll(body, "\n", "\r\n"); // RFC 6068: line breaks as CRLF
    query.push_back("body=" + percentEncode(body));
  }
  if (!query.empty())
    url += "?" + util::join(query, "&");
  return url;
}

std::optional<util::fs::path> findReaderEvaluation(const util::fs::path &serviceFolder) {
  const auto folder = serviceFolder / util::pathFromUtf8(kEvaluationFolder);
  std::optional<util::fs::path> best;
  std::int64_t bestTime = 0;
  for (const auto &file : util::listDirectory(folder, false, true)) {
    if (util::toLower(util::pathToUtf8(file.extension())) != ".pdf")
      continue;
    if (!util::containsIgnoreCase(util::pathToUtf8(file.filename()), kEvaluationName))
      continue;
    const auto time = util::lastWriteTime(file);
    if (!best || time > bestTime) {
      best = file;
      bestTime = time;
    }
  }
  return best;
}

util::Status MailTemplateStore::load() {
  if (!util::pathExists(mFile)) {
    mMail = defaultMailTemplate();
    mLastWrite = 0;
    return {};
  }
  auto j = util::json::load(mFile);
  if (!j)
    return std::unexpected(j.error());
  if (!j->is_object())
    return std::unexpected(util::Error{"E-Mail-Vorlage muss ein JSON-Objekt sein: " + util::pathToUtf8(mFile)});
  const MailTemplate defaults = defaultMailTemplate();
  mMail.to = util::json::str(*j, "to");
  mMail.cc = util::json::str(*j, "cc");
  mMail.subject = util::json::str(*j, "subject", defaults.subject);
  mMail.body = util::json::str(*j, "body", defaults.body);
  mLastWrite = util::lastWriteTime(mFile);
  return {};
}

util::Status MailTemplateStore::save(const MailTemplate &mail) {
  util::json::Json j;
  // keep keys written by newer program versions
  if (util::pathExists(mFile))
    if (auto existing = util::json::load(mFile); existing && existing->is_object())
      j = std::move(*existing);
  j["to"] = mail.to;
  j["cc"] = mail.cc;
  j["subject"] = mail.subject;
  j["body"] = mail.body;
  if (auto s = util::json::save(mFile, j); !s)
    return s;
  mMail = mail;
  mLastWrite = util::lastWriteTime(mFile);
  return {};
}

} // namespace st::exporting
