// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Access Linea's recent files.
 *
 * Copyright 2025 Martin Owens <doctormo@geek-2.com>
 * Copyright 2026 Authors

 */

#ifndef LINEA_IO_RECENT_FILES_H
#define LINEA_IO_RECENT_FILES_H

#include <map>
#include <optional>
#include <string>
#include <vector>
#include <glibmm/ustring.h>

namespace Linea::IO {

/// A single recent-file entry. Toolkit-agnostic
struct RecentFile {
    std::string uri;                     ///< file URI (file://…)
    std::string display_name;            ///< user-visible name (document title or basename)
    std::string path;                    ///< absolute filesystem path
    std::vector<std::string> groups;     ///< e.g. "Auto", "Crash"
    std::optional<std::string> original; ///< original filename for autosave/crash entries
    long modified = 0;                   ///< last-modified Unix timestamp
};

/// Generate a vector of recently used files, most-recent first.
///
/// @arg max_files  Limits the output to this number of files; 0 means no limit.
/// @arg is_autosave  If true, return only auto-save and crash-recovery entries.
std::vector<RecentFile> getRecentFiles(unsigned max_files = 0, bool is_autosave = false);

/// Add (or promote) a recent SVG file.
///
/// @arg filename  Absolute local filesystem path of the document.
/// @arg name      Display name (document title); may be empty.
/// @arg groups    Optional groups, e.g. "Auto" for auto-save, "Crash" for crash recovery.
/// @arg original  Original filename for auto-save/crash entries; marks the entry private.
void addInkscapeRecentSvg(const std::string& filename, const std::string& name, std::vector<std::string> groups = {},
                          std::optional<std::string> original = {});

/// Remove a recent file entry (call when deleting files).
void removeInkscapeRecent(const std::string& filename);

/// Remove all Inkscape recent entries (preserves nothing — used by "clear recent" action).
void resetRecentInkscapeList();

/// Look up the original filename for an auto-save / crash file.
///
/// @returns nullopt if the file is not a recent auto-save/crash entry;
///          an empty string if it is but has no original (was unsaved);
///          otherwise the original filename.
std::optional<std::string> openAsInkscapeRecentOriginalFile(const std::string& filename);

/// Generate shortened display labels for a list of recent files,
/// disambiguating entries that share the same basename.
std::map<std::string, std::string> getShortenedPathMap(const std::vector<RecentFile>& recent_files);

} // namespace Linea::IO

#endif // LINEA_IO_RECENT_FILES_H
