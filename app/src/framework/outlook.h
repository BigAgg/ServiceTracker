#pragma once

#include <st/exporting/mail.h>

/// Mail drafts in Microsoft Outlook (classic) via COM automation.
namespace st::app::outlook {

/// Opens a new, unsent mail in Outlook with recipients, subject, text and attachments.
/// The text is put in front of the user's default signature. Fails if Outlook (classic)
/// is not installed – the new Outlook has no automation interface. Starts Outlook if
/// necessary, so call it from a worker thread (it initialises COM itself).
util::Status createDraft(const exporting::MailDraft &draft);

} // namespace st::app::outlook
