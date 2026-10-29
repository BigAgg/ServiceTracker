#pragma once

#include "st/checklist/model.h"
#include "st/translate/translator.h"

/// Translation of a checklist for the English service record.
namespace st::checklist {

/// Translates everything textual a record shows: names of sections, task groups,
/// tables, columns and tasks, values of text fields/text boxes and the selected combo
/// entry. Numbers, dates, check marks, user names and timestamps stay unchanged.
/// On error the checklist is left untouched.
util::Status translateChecklist(Checklist &checklist, translate::Translator &translator);

} // namespace st::checklist
