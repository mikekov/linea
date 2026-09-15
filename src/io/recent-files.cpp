// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Create a list of recently used files.
 *
 * Backed by Inkscape::Preferences — no Gtk::RecentManager dependency.
 *
 * Storage layout in preferences:
 *   /recentfiles/count        — number of stored entries
 *   /recentfiles/entryN/uri   — file URI
 *   /recentfiles/entryN/name  — display name (document title)
 *   /recentfiles/entryN/groups — comma-separated group tags ("Auto","Crash")
 *   /recentfiles/entryN/original — original filename (autosave/crash only)
 *   /recentfiles/entryN/modified — Unix timestamp (string)
 *
 * Copyright 2025 Martin Owens <doctormo@geek-2.com>
 * Copyright 2024, 2025 Tavmjong Bah <tavmjong@free.fr>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "recent-files.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstring>
#include <exception>
#include <format>
#include <glibmm/convert.h>
#include <glibmm/miscutils.h>

#include "io/split-path.h"
#include "preferences.h"

namespace Linea::IO {

namespace {

#ifdef _WIN32
constexpr size_t platform_index = 1;
#else
constexpr size_t platform_index = 0;
#endif

// Maximum number of entries to persist (matches the preference default).
constexpr int MAX_STORED = 100;

// --- Preferences helpers ---

Glib::ustring entry_path(int index, const char* field) {
    return Glib::ustring::compose("/recentfiles/entry%1/%2", index, field);
}

int get_count() {
    return Inkscape::Preferences::get()->getInt("/recentfiles/count", 0);
}

void set_count(int n) {
    Inkscape::Preferences::get()->setInt("/recentfiles/count", n);
}

std::string get_field(int index, const char* field, const std::string& def = "") {
    auto val = Inkscape::Preferences::get()->getString(entry_path(index, field), Glib::ustring(def));
    return val.raw();
}

std::string make_valid_utf8(const std::string& value) {
    if (g_utf8_validate(value.c_str(), -1, nullptr)) return value;

    auto valid = g_utf8_make_valid(value.c_str(), -1);
    std::string result{valid};
    g_free(valid);
    return result;
}

void set_field(int index, const char* field, const std::string& value) {
    auto valid = make_valid_utf8(value);
    Inkscape::Preferences::get()->setString(entry_path(index, field), Glib::ustring(valid));
}

std::string filename_to_uri(const std::string& filename) {
    try {
        return Glib::filename_to_uri(filename).raw();
    } catch (const Glib::ConvertError&) {
        return {};
    }
}

std::string uri_to_filename(const std::string& uri) {
    try {
        return Glib::filename_from_uri(uri);
    } catch (const Glib::ConvertError&) {
        return {};
    }
}

std::string get_entry_uri(int index) {
    auto uri = get_field(index, "uri");
    if (uri.empty()) {
        // Read entries written by older versions, which stored a filesystem path.
        uri = filename_to_uri(get_field(index, "path"));
    }
    return uri;
}

void clear_entry(int index) {
    auto prefs = Inkscape::Preferences::get();
    prefs->setString(entry_path(index, "uri"), "");
    prefs->setString(entry_path(index, "name"), "");
    prefs->setString(entry_path(index, "groups"), "");
    prefs->setString(entry_path(index, "original"), "");
    prefs->setString(entry_path(index, "modified"), "");
}

// --- Serialization helpers ---

std::string join_groups(const std::vector<std::string>& groups) {
    std::string out;
    for (size_t i = 0; i < groups.size(); ++i) {
        if (i) out += ',';
        out += groups[i];
    }
    return out;
}

std::vector<std::string> split_groups(const std::string& s) {
    std::vector<std::string> out;
    size_t start = 0;
    for (size_t i = 0; i <= s.size(); ++i) {
        if (i == s.size() || s[i] == ',') {
            if (i > start) out.emplace_back(s, start, i - start);
            start = i + 1;
        }
    }
    return out;
}

bool has_group(const std::vector<std::string>& groups, const std::string& tag) {
    return std::find(groups.begin(), groups.end(), tag) != groups.end();
}

// Read a single entry from preferences by index.
std::optional<RecentFile> read_entry(int index) {
    RecentFile rf;
    rf.uri = get_entry_uri(index);
    rf.path = uri_to_filename(rf.uri);
    if (rf.path.empty() || !g_file_test(rf.path.c_str(), G_FILE_TEST_IS_REGULAR)) {
        return std::nullopt;
    }

    rf.display_name = get_field(index, "name");
    rf.groups = split_groups(get_field(index, "groups"));

    auto original = get_field(index, "original");
    if (original.starts_with("file:")) {
        original = uri_to_filename(original);
    }
    rf.original = original.empty() ? std::nullopt : std::optional{std::move(original)};

    try {
        rf.modified = std::stol(get_field(index, "modified", "0"));
    } catch (const std::exception&) {
        return std::nullopt;
    }

    if (rf.display_name.empty()) {
        rf.display_name = Glib::path_get_basename(rf.path);
    }
    return rf;
}

// Find the index of an entry by file URI, or -1 if not found.
int find_by_uri(const std::string& uri) {
    int count = get_count();
    for (int i = 0; i < count; ++i) {
        if (get_entry_uri(i) == uri) return i;
    }
    return -1;
}

// Shift entries [index .. count-1] down by one, overwriting `index`.
void shift_down(int index, int count) {
    for (int i = index; i + 1 < count; ++i) {
        set_field(i, "uri", get_entry_uri(i + 1));
        set_field(i, "name", get_field(i + 1, "name"));
        set_field(i, "groups", get_field(i + 1, "groups"));
        set_field(i, "original", get_field(i + 1, "original"));
        set_field(i, "modified", get_field(i + 1, "modified"));
    }
}

void remove_entry_at(int index, int count) {
    shift_down(index, count);
    clear_entry(count - 1);
    set_count(count - 1);
}

} // unnamed namespace

std::vector<RecentFile> getRecentFiles(unsigned max_files, bool is_autosave) {
    int count = get_count();
    std::vector<RecentFile> entries;
    entries.reserve(count);

    for (int i = 0; i < count;) {
        auto rf = read_entry(i);
        if (!rf) {
            remove_entry_at(i, count);
            --count;
            continue;
        }
        auto is_recovery = has_group(rf->groups, "Auto") || has_group(rf->groups, "Crash");
        if (is_autosave != is_recovery) {
            ++i;
            continue;
        }
        entries.push_back(std::move(*rf));
        ++i;
    }

    // Sort by modified time, most-recent first.
    std::sort(entries.begin(), entries.end(),
              [](const RecentFile& a, const RecentFile& b) { return a.modified > b.modified; });

    if (max_files && entries.size() > max_files) {
        entries.resize(max_files);
    }

    return entries;
}

void addInkscapeRecentSvg(const std::string& filename, const std::string& name, std::vector<std::string> groups,
                          std::optional<std::string> original) {
    if (filename.empty() || !Glib::path_is_absolute(filename)) return;

    auto uri = filename_to_uri(filename);
    if (uri.empty()) return;
    auto original_uri = original ? filename_to_uri(*original) : std::string{};
    if (original && original_uri.empty()) return;

    int count = get_count();

    // If this file already exists, remove it so it can be re-inserted at front.
    int existing = find_by_uri(uri);
    if (existing >= 0) {
        shift_down(existing, count);
        --count;
    }

    // Make room: if at capacity, drop the oldest (last) entry.
    if (count >= MAX_STORED) {
        clear_entry(count - 1);
        --count;
    }

    // Insert at front: shift everything up by one.
    for (int i = count; i > 0; --i) {
        set_field(i, "uri", get_entry_uri(i - 1));
        set_field(i, "name", get_field(i - 1, "name"));
        set_field(i, "groups", get_field(i - 1, "groups"));
        set_field(i, "original", get_field(i - 1, "original"));
        set_field(i, "modified", get_field(i - 1, "modified"));
    }

    auto now = std::chrono::system_clock::now();
    auto ts = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

    set_field(0, "uri", uri);
    set_field(0, "name", name);
    set_field(0, "groups", join_groups(groups));
    set_field(0, "original", original_uri);
    set_field(0, "modified", std::to_string(ts));

    set_count(count + 1);
}

void removeInkscapeRecent(const std::string& filename) {
    auto uri = filename_to_uri(filename);
    if (uri.empty()) return;

    int count = get_count();
    int idx = find_by_uri(uri);
    if (idx < 0) return;

    remove_entry_at(idx, count);
}

void resetRecentInkscapeList() {
    int count = get_count();
    for (int i = 0; i < count; ++i) {
        clear_entry(i);
    }
    set_count(0);
}

std::optional<std::string> openAsInkscapeRecentOriginalFile(const std::string& filename) {
    auto uri = filename_to_uri(filename);
    if (uri.empty()) return std::nullopt;

    int idx = find_by_uri(uri);
    if (idx < 0) return std::nullopt;

    auto groups = split_groups(get_field(idx, "groups"));
    auto original = get_field(idx, "original");
    if (original.starts_with("file:")) {
        original = uri_to_filename(original);
    }
    if (has_group(groups, "Auto")) {
        return original;
    }
    if (has_group(groups, "Crash")) {
        // Crash files are removed from the recent list on opening.
        removeInkscapeRecent(filename);
        return original;
    }
    return std::nullopt;
}

std::map<std::string, std::string> getShortenedPathMap(const std::vector<RecentFile>& recent_files) {
    // Prefill with display names.
    std::map<std::string, std::string> shortened;
    for (const auto& rf : recent_files) {
        shortened[rf.path] = rf.display_name;
    }

    // Look for duplicate basenames — those need disambiguation.
    auto by_basename = recent_files;
    std::sort(by_basename.begin(), by_basename.end(), [](const RecentFile& a, const RecentFile& b) {
        return Glib::path_get_basename(a.path) < Glib::path_get_basename(b.path);
    });

    for (size_t i = 0; i + 1 < by_basename.size(); ++i) {
        auto& a = by_basename[i];
        auto& b = by_basename[i + 1];
        if (Glib::path_get_basename(a.path) != Glib::path_get_basename(b.path)) continue;

        // Found a duplicate basename — find first differing directory from the root.
        auto parts_a = Inkscape::IO::split_path(a.path);
        auto parts_b = Inkscape::IO::split_path(b.path);
        auto max_size = std::min(parts_a.size(), parts_b.size());
        size_t diff = 0;
        for (; diff < max_size; ++diff) {
            if (parts_a[diff] != parts_b[diff]) break;
        }

        // Disambiguate both entries.
        for (auto* parts : {&parts_a, &parts_b}) {
            auto& p = *parts;
            auto size = p.size();
            auto path = (parts == &parts_a) ? a.path : b.path;

            if (size <= 3) {
                shortened[path] = path;
            } else if (diff == size - 1) {
                shortened[path] = std::string(p.basename());
            } else if (diff == size - 2) {
                shortened[path] = std::format("..{0}{1}{0}{2}", G_DIR_SEPARATOR, p[size - 2], p[size - 1]);
            } else if (diff <= platform_index) {
                shortened[path] =
                    std::format("{0}{1}{2}{1}..{1}{3}", p.prefix(), G_DIR_SEPARATOR, p[platform_index], p[size - 1]);
            } else {
                shortened[path] = std::format("..{0}{1}{0}..{0}{2}", G_DIR_SEPARATOR, p[diff], p[size - 1]);
            }
        }
    }

    return shortened;
}

} // namespace Linea::IO
