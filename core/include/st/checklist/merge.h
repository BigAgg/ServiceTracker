#pragma once

#include "st/checklist/model.h"

#include <cstddef>

/// Merging of concurrent edits.
///
/// Several users edit the same checklist file on the network drive at the same time.
/// Before saving (and whenever the file changes on disk) the version on disk is merged
/// into the local one. The rule is "newest timestamp wins", applied per element:
///
///  - checklist / section finish state: newer timestamp wins
///  - task: the version with the newer timestamp wins (value, comment, attachments ...)
///  - task group: tasks are merged pairwise if both sides have the same number of tasks
///  - table: the side with the newer table timestamp defines the rows (add/remove/move),
///    afterwards cells are merged pairwise by row index
///  - sections are matched by name; sections only present locally stay untouched
///
/// Structural changes made in the editor (different number of items) are never merged
/// item by item; the local structure is kept in that case.
namespace st::checklist {

/// Merges all elements of `remote` that are newer into `local`.
/// Returns the number of changed elements (0 = nothing changed).
std::size_t mergeNewer(Checklist &local, const Checklist &remote);

} // namespace st::checklist
