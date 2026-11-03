// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Document export helpers.
 */

#include "export-helpers.h"

#include <cmath>
#include <cstring>
#include <set>
#include <glibmm/convert.h>
#include <glibmm/fileutils.h>
#include <glibmm/i18n.h>
#include <glibmm/miscutils.h>
#include <sys/stat.h>
#include <unistd.h>

#include "colors/color.h"
#include "desktop.h"
#include "document.h"
#include "extension/output.h"
#include "helper/png-write.h"
#include "inkscape.h"
#include "io/resource.h"
#include "io/sys.h"
#include "object/object-set.h"
#include "object/sp-item.h"
#include "object/sp-page.h"
#include "object/sp-root.h"
#include "object/weakptr.h"
#include "page-manager.h"
#include "selection.h"
#include "util/cast.h"
#include "ui/desktop/desktop-widget.h"
#include "ui/interface.h"
#include "ui/util.h"

namespace Linea::IO {

namespace {

void exportError(SPDesktop* desktop, const Glib::ustring& error) {
    auto widget = desktop ? desktop->getDesktopWidget() : nullptr;
    if (!widget) {
        return;
    }
    widget->showError(QString::fromUtf8(_("Export")), QString::fromUtf8(error.c_str()));
}

// Make an object label safe to use as the base of a filename.
std::string sanitizeFileName(std::string name) {
    for (auto& c : name) {
        if (std::strchr("/\\:*?\"<>|", c)) {
            c = '_';
        }
    }
    auto first = name.find_first_not_of(" \t\n");
    auto last = name.find_last_not_of(" \t\n");
    name = (first == std::string::npos) ? std::string() : name.substr(first, last - first + 1);
    while (!name.empty() && name.front() == '.') {
        name.erase(name.begin());
    }
    return name.empty() ? "export" : name;
}

} // namespace

std::string absolutizePath(SPDocument* doc, const std::string& filename) {
    std::string path;
    // Make relative paths go from the document location, if possible:
    if (!Glib::path_is_absolute(filename) && doc->getDocumentFilename()) {
        auto dirname = Glib::path_get_dirname(doc->getDocumentFilename());
        if (!dirname.empty()) {
            path = Glib::build_filename(dirname, filename);
        }
    }
    if (path.empty()) {
        path = filename;
    }
    return path;
}

bool unConflictFilename(SPDocument* doc, std::string& filename, const std::string extension) {
    std::string path = absolutizePath(doc, filename);
    std::string test_filename = path + extension;
    if (!Inkscape::IO::file_test(test_filename.c_str(), G_FILE_TEST_EXISTS)) {
        filename = test_filename;
        return true;
    }
    for (int i = 1; i <= 100; i++) {
        if (i == 1) {
            test_filename = path + " copy" + extension;
        } else {
            test_filename = path + " copy " + std::to_string(i) + extension;
        }
        if (!Inkscape::IO::file_test(test_filename.c_str(), G_FILE_TEST_EXISTS)) {
            filename = test_filename;
            return true;
        }
    }
    return false;
}

/**
 * Checks if the directory exists and if not, tries to create the directory and if failed, displays an error message.
 *
 * @arg filename - File path (may be absolute or relative to current document).
 * Value is in platform-native encoding (see Glib::filename_to_utf8).
 */
bool checkOrCreateDirectory(const std::string& filename) {
    SPDesktop* desktop = SP_ACTIVE_DESKTOP;
    if (!desktop) {
        return false;
    }

    std::string path = absolutizePath(desktop->getDocument(), filename);
    std::string dirname = Glib::path_get_dirname(path);

    if (dirname.empty() ||
        !Inkscape::IO::file_test(dirname.c_str(), (GFileTest)(G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR))) {
        if (g_mkdir_with_parents(dirname.c_str(), S_IRWXU) != 0) {
            Glib::ustring safeDir = Inkscape::IO::sanitizeString(dirname.c_str());
            auto error =
                Glib::ustring::compose(_("Directory <b>%1</b> does not exist and can't be created.\n"), safeDir);
            exportError(desktop, error);
            return false;
        }
    }
    return true;
}

/**
 * Export to raster graphics
 *
 * @arg filename Filename. Path is absolute or relative to the current document.
 * Value is in UTF8 encoding
 */
bool exportRaster(const Geom::Rect& area, const unsigned long int& width, const unsigned long int& height,
                  const float& dpi, const Inkscape::Colors::Color& bgcolor, const Glib::ustring& filename,
                  bool overwrite, unsigned (*callback)(float, void*), void* data,
                  Inkscape::Extension::Output* extension, std::vector<const SPItem*>* items) {
    SPDesktop* desktop = SP_ACTIVE_DESKTOP;
    if (!desktop) return false;
    SPDocument* doc = desktop->getDocument();

    if (area.hasZeroArea() || width == 0 || height == 0) {
        exportError(desktop, _("The chosen area to be exported is invalid."));
        return false;
    }
    if (filename.empty()) {
        exportError(desktop, _("You have to enter a filename."));
        return false;
    }

    if (!extension || !extension->is_raster()) {
        exportError(desktop, _("Raster export Method is used for NON RASTER EXTENSION"));
        return false;
    }

    float pHYs = extension->get_param_float("png_phys", dpi);
    if (pHYs < 0.01) pHYs = dpi;

    bool use_interlacing = extension->get_param_bool("png_interlacing", false);
    int antialiasing = extension->get_param_int("png_antialias", 2); // Cairo anti aliasing
    int zlib = extension->get_param_int("png_compression", 1);       // Default is 6 for png, but 1 for non-png
    auto val = extension->get_param_int("png_bitdepth", 99);         // corresponds to RGBA 8

    int bit_depth = pow(2, (val & 0x0F));
    int color_type = (val & 0xF0) >> 4;

    std::string path = absolutizePath(doc, Glib::filename_from_utf8(filename));

    // Do the over-write protection now, since the png is just a temp file.
    if (!overwrite && !sp_ui_overwrite_file(path)) {
        return false;
    }

    auto fn = Glib::path_get_basename(path);
    /// Path to temporary PNG file.
    /// Value is in platform-native encoding (see Glib::filename_to_utf8).
    auto png_filename = path;
    {
        // Select the extension and set the filename to a temporary file
        int tempfd_out = Glib::file_open_tmp(png_filename, "ink_ext_");
        close(tempfd_out);
    }

    // Export Start Here
    std::vector<const SPItem*> selected;
    if (items && items->size() > 0) {
        selected = *items;
    }

    ExportResult result = sp_export_png_file(
        desktop->getDocument(), Glib::filename_to_utf8(png_filename).c_str(), area, width, height, pHYs,
        pHYs, // previously xdpi, ydpi.
        bgcolor, callback, data, true, selected, use_interlacing, color_type, bit_depth, zlib, antialiasing);

    bool failed = result == EXPORT_ERROR; // || prog_dialog->get_stopped();

    if (failed) {
        Glib::ustring safeFile = Inkscape::IO::sanitizeString(Glib::filename_to_utf8(path).c_str());
        Glib::ustring error = Glib::ustring::compose(_("Could not export to filename <b>%1</b>.\n"), safeFile);

        exportError(desktop, error);
        return false;
    } else if (result == EXPORT_OK) {
        // Don't ask for preferences on every run.
        try {
            extension->export_raster(doc, png_filename, path.c_str(), false);
        } catch (Inkscape::Extension::Output::save_failed& e) {
            return false;
        }
    } else {
        // Extensions have their own error popup, so this only tracks failures in the png step
        desktop->messageStack()->flash(Inkscape::INFORMATION_MESSAGE, _("Export aborted."));
        return false;
    }

    Glib::ustring safeFile = Inkscape::IO::sanitizeString(path.c_str());
    desktop->messageStack()->flashF(Inkscape::INFORMATION_MESSAGE, _("Drawing exported to <b>%s</b>."),
                                    safeFile.c_str());

    unlink(png_filename.c_str());
    return true;
}

bool exportVector(Inkscape::Extension::Output* extension, SPDocument* doc, const Glib::ustring& filename,
                  bool overwrite, const Geom::Rect& area) {
    doc->ensureUpToDate();
    doc->getPageManager().disablePages();
    doc->fitToRect(area, false);
    doc->getPageManager().enablePages();
    auto page = doc->getPageManager().getFirstPage();
    return exportVector(extension, doc, filename, overwrite, {}, page);
}

bool exportVector(Inkscape::Extension::Output* extension, SPDocument* doc, const Glib::ustring& filename,
                  bool overwrite, const std::vector<const SPItem*>& items, const SPPage* page) {
    std::vector<const SPPage*> pages;
    if (page) pages.push_back(page);
    return exportVector(extension, doc, filename, overwrite, items, pages);
}

/**
 * Export to vector graphics
 *
 * @arg filename Filename. Path is absolute or relative to the current document.
 * Value is in UTF8 encoding.
 */
bool exportVector(Inkscape::Extension::Output* extension, SPDocument* copy_doc, const Glib::ustring& filename,
                  bool overwrite, const std::vector<const SPItem*>& items, const std::vector<const SPPage*>& pages) {
    SPDesktop* desktop = SP_ACTIVE_DESKTOP;
    if (!desktop) return false;

    if (filename.empty()) {
        exportError(desktop, _("You have to enter a filename."));
        return false;
    }

    if (!extension || extension->is_raster()) {
        exportError(desktop, _("Vector export Method is used for RASTER EXTENSION"));
        return false;
    }

    std::string path = absolutizePath(copy_doc, Glib::filename_from_utf8(filename));
    Glib::ustring safeFile = Inkscape::IO::sanitizeString(path.c_str());

    // Do the over-write protection now
    if (!overwrite && !sp_ui_overwrite_file(path)) {
        return false;
    }
    copy_doc->ensureUpToDate();

    std::vector<const SPItem*> objects = items;
    std::set<std::string> obj_ids;
    std::set<std::string> page_ids;
    for (auto page : pages) {
        if (auto _id = page->getId()) {
            page_ids.insert(std::string(_id));
        }
        // If page then our item set is limited to the overlapping items
        auto page_items = page->getOverlappingItems(true, true);

        if (items.empty()) {
            // Items is page_items, remove all items not in this page.
            objects.insert(objects.end(), page_items.begin(), page_items.end());
        } else {
            for (auto& item : page_items) {
                item->getIds(obj_ids);
            }
        }
    }

    // Delete any pages not specified, delete all pages if none specified
    auto& pm = copy_doc->getPageManager();

    // Make weak pointers to pages, since deletePage() can delete more than just the requested page.
    std::vector<SPWeakPtr<SPPage>> copy_pages;
    copy_pages.reserve(pm.getPageCount());
    for (auto page : pm.getPages()) {
        copy_pages.emplace_back(page);
    }

    // We refuse to delete anything if everything would be deleted.
    for (auto& page : copy_pages) {
        if (page) {
            auto _id = page->getId();
            if (_id && page_ids.find(_id) == page_ids.end()) {
                pm.deletePage(page.get(), false);
            }
        }
    }

    // Page export ALWAYS restricts, even if nothing would be on the page.
    if (!objects.empty() || !pages.empty()) {
        std::vector<SPObject*> objects_to_export;
        Inkscape::ObjectSet object_set(copy_doc);
        for (auto& object : objects) {
            auto _id = object->getId();
            if (!_id || (!obj_ids.empty() && obj_ids.find(_id) == obj_ids.end())) {
                // This item is off the page so can be ignored for export
                continue;
            }

            SPObject* obj = copy_doc->getObjectById(_id);
            if (!obj) {
                Glib::ustring error =
                    Glib::ustring::compose(_("Could not export to filename <b>%1</b>. (missing object)\n"), safeFile);

                exportError(desktop, error);

                return false;
            }
            copy_doc->ensureUpToDate();

            object_set.add(obj, true);
            objects_to_export.push_back(obj);
        }

        copy_doc->getRoot()->cropToObjects(objects_to_export);

        if (pages.empty()) {
            object_set.fitCanvas(false, true);
        }
    }

    // Remove all unused definitions
    copy_doc->vacuumDocument();

    try {
        extension->save(copy_doc, path.c_str());
    } catch (Inkscape::Extension::Output::save_failed& e) {
        Glib::ustring safeFile = Inkscape::IO::sanitizeString(path.c_str());
        Glib::ustring error = Glib::ustring::compose(_("Could not export to filename <b>%1</b>.\n"), safeFile);

        exportError(desktop, error);

        return false;
    }

    desktop->messageStack()->flashF(Inkscape::INFORMATION_MESSAGE, _("Drawing exported to <b>%s</b>."),
                                    safeFile.c_str());
    return true;
}

std::string filePathFromObject(SPDocument* doc, SPObject* obj, const std::string& file_entry_text) {
    Glib::ustring id = _("bitmap");
    if (obj && obj->getId()) {
        id = obj->getId();
    }
    return prependDirectory(Glib::filename_from_utf8(id), file_entry_text, doc);
}

/**
 * Adds the full directory path to the final part of a file name.
 *
 * @arg name - The final name part of a path, if absolute path, this is returned unchanged.
 *             Value is in platform-native encoding (see Glib::filename_to_utf8).
 * @arg orig - The original filename, an optional path used previously which this name
 *             will be appanded in order to produce the full path.
 *             Value is in platform-native encoding (see Glib::filename_to_utf8).
 * @arg doc - The document to take a fallback directory from if orig is empty.
 *
 * @returns
 *  A path ending with `name` and starting with the directory part of
 *    1. `name` (if it is an absolute path or empty)
 *    2. `orig`
 *    3. `doc`
 *    or
 *    4. the user home directory
 *  Value is in platform-native encoding (see Glib::filename_to_utf8).
 */
std::string prependDirectory(const std::string& name, const std::string& orig, SPDocument* doc) {
    if (Glib::path_is_absolute(name) || name.empty()) return name;

    if (Glib::path_get_basename(name) != name) {
        return name;
    }

    std::string directory;

    if (!orig.empty()) {
        directory = Glib::path_get_dirname(orig);
        // Note: Glib::path_get_dirname() returns "." if there is no directory component
    }
    if (directory.empty() || directory == ".") {
        /* Grab document directory */
        const gchar* docFilename = doc->getDocumentFilename();
        if (docFilename) {
            directory = Glib::path_get_dirname(docFilename);
        }
    }
    if (directory.empty() || directory == ".") {
        directory = Inkscape::IO::Resource::homedir_path();
    }

    return Glib::build_filename(directory, name);
}

std::string defaultFilename(SPDocument* doc, const std::string& filename_entry_text, const std::string& extension) {
    std::string filename;
    if (doc && doc->getDocumentFilename()) {
        filename = doc->getDocumentFilename();
    } else if (doc) {
        filename = prependDirectory(Glib::filename_from_utf8(_("bitmap")), filename_entry_text, doc);
        filename = filename + extension;
    }
    return filename;
}

void set_export_bg_color(SPObject* object, const Inkscape::Colors::Color& color) {
    if (object) {
        object->setAttribute("inkscape:export-bgcolor", color.toString());
    }
}

Inkscape::Colors::Color get_export_bg_color(SPObject* object, const Inkscape::Colors::Color& default_color) {
    if (object) {
        if (auto c = Inkscape::Colors::Color::parse(object->getAttribute("inkscape:export-bgcolor"))) {
            return *c;
        }
    }
    return default_color;
}

std::vector<std::string> exportSelection(SPDesktop* desktop, const std::vector<ExportJob>& jobs) {
    std::vector<std::string> exported;
    if (!desktop) {
        return exported;
    }
    auto doc = desktop->getDocument();
    auto selection = desktop->getSelection();
    if (!doc || !selection) {
        return exported;
    }

    // one pass over objects(): each entry is one exportable thing — item,
    // page, or the <svg> root (whole document)
    struct Entry {
        const SPObject* object = nullptr;
        const SPItem* item = nullptr;
        const SPPage* page = nullptr;
        Geom::OptRect bounds;
    };
    std::vector<Entry> entries;
    Geom::OptRect area;
    auto unite = [&area](const Geom::Rect& r) {
        area = area ? Geom::Rect(*area | r) : r;
    };
    for (auto object : selection->objects()) {
        if (auto page = cast<SPPage>(object)) {
            entries.push_back({object, nullptr, page, page->getDesktopRect()});
            unite(page->getDesktopRect());
        } else if (auto item = cast<SPItem>(object)) {
            if (is<SPRoot>(item)) {
                entries.push_back({object, nullptr, nullptr, doc->preferredBounds()});
                if (auto bounds = doc->preferredBounds()) {
                    unite(*bounds);
                }
            } else {
                entries.push_back({object, item, nullptr, item->documentVisualBounds()});
                if (auto r = item->documentVisualBounds()) {
                    unite(*r);
                }
            }
        }
    }
    if (entries.empty() || !area || area->hasZeroArea()) {
        exportError(desktop, _("Nothing selected to export."));
        return exported;
    }

    // directory: the document's folder, or temp when it hasn't been saved
    std::string dir;
    if (auto filename = doc->getDocumentFilename()) {
        dir = Glib::path_get_dirname(filename);
    }
    if (dir.empty()) {
        dir = g_get_tmp_dir();
    }

    // a single object exports as one file named after it; multiple objects go
    // into a "Selection" directory (unconflicted), one file per object
    std::vector<std::pair<Entry, std::string>> targets;
    auto name_of = [](const SPObject* object) {
        const char* label = object->label();
        return sanitizeFileName(label ? label : get_synthetic_object_name(object).toStdString());
    };
    if (entries.size() == 1) {
        targets.push_back({entries.front(), name_of(entries.front().object)});
    } else {
        std::string sdir = dir + "/" + "Selection";
        if (!unConflictFilename(doc, sdir, "")) {
            return exported;
        }
        dir = std::move(sdir);
        for (auto& entry : entries) {
            targets.push_back({entry, name_of(entry.object)});
        }
    }

    // page color from the named view; export-bgcolor attribute may override it
    Inkscape::Colors::Color bgcolor(Inkscape::Colors::Space::Type::RGB, {0, 0, 0, 0});
    if (auto nv = doc->getReprNamedView()) {
        if (auto c = Inkscape::Colors::Color::parse(nv->attribute("pagecolor"))) {
            bgcolor = *c;
        }
    }
    bgcolor = get_export_bg_color(doc->getRoot(), bgcolor);

    for (auto& job : jobs) {
        auto extension = job.extension;
        if (!extension) {
            continue;
        }

        for (auto& [entry, name] : targets) {
            // resolve a unique path; the extension supplies the suffix
            std::string path = dir + "/" + name;
            if (!unConflictFilename(doc, path, extension->get_extension())) {
                return exported;
            }
            if (!checkOrCreateDirectory(path)) {
                return exported;
            }
            const auto filename = Glib::filename_to_utf8(path);

            bool ok;
            if (extension->is_raster()) {
                const auto dpi = static_cast<float>(job.dpi);
                const auto& bounds = entry.bounds ? *entry.bounds : *area;
                const auto width = static_cast<unsigned long>(bounds.width() * dpi / 96.0 + 0.5);
                const auto height = static_cast<unsigned long>(bounds.height() * dpi / 96.0 + 0.5);
                std::vector<const SPItem*> items;
                if (entry.item) {
                    items.push_back(entry.item);
                }
                ok = exportRaster(bounds, width, height, dpi, bgcolor, filename, true, nullptr, nullptr,
                                  extension, entry.item ? &items : nullptr);
            } else {
                auto copy = doc->copy();
                std::vector<const SPItem*> items;
                std::vector<const SPPage*> pages;
                if (entry.item) {
                    items.push_back(entry.item);
                }
                if (entry.page) {
                    pages.push_back(entry.page);
                }
                ok = exportVector(extension, copy.get(), filename, true, items, pages);
            }
            if (!ok) {
                return exported; // first error stops all processing
            }
            exported.push_back(path);
        }
    }
    return exported;
}

} // namespace Linea::IO
