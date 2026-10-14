#pragma once

#include "st/checklist/model.h"
#include "st/util/fs.h"

#include <cstdint>
#include <string>
#include <string_view>

/// Serialisation of checklists. Both formats are 100% compatible with the legacy
/// program: files written here can be opened by older versions and vice versa.
///
/// Legacy text format (*.txt):  KEY=value lines with *_START / *_END blocks.
/// JSON format (*.json):        see docs/dateiformate.md.
namespace st::checklist {

enum class Format { LegacyText,
                    Json };

/// *.json -> Json, everything else -> LegacyText.
Format formatForPath(const util::fs::path &path);

/// Parses the legacy text format. Unknown lines are ignored, so the parser never fails.
Checklist parseLegacyText(std::string_view content);

/// Writes the legacy text format.
std::string writeLegacyText(const Checklist &checklist);

/// Parses the JSON format.
util::Result<Checklist> parseJson(std::string_view content);

/// Writes the JSON format. `attachmentsDir` and `lastEdit` are informational
/// fields the legacy program wrote; they are not read back.
std::string writeJson(const Checklist &checklist, std::string_view attachmentsDir = {}, std::int64_t lastEdit = 0);

/// Parses `content` in the given format.
util::Result<Checklist> parse(std::string_view content, Format format);

/// Writes `checklist` in the given format.
std::string write(const Checklist &checklist, Format format);

/// Converts the legacy post action name "resolve_rinfo" into "auto_finish".
std::string normalizePostAction(std::string_view name);

} // namespace st::checklist
