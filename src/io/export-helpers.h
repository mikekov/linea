// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Document export helpers.
 * Output paths are constructed by the caller.
 */

#ifndef LINEA_IO_EXPORT_HELPERS_H
#define LINEA_IO_EXPORT_HELPERS_H

#include <string>
#include <vector>
#include <glibmm/ustring.h>
#include <2geom/rect.h>

class SPDesktop;
class SPDocument;
class SPItem;
class SPObject;
class SPPage;

namespace Inkscape::Colors {
class Color;
}

namespace Inkscape::Extension {
class Output;
}

namespace Linea::IO {

// Make relative paths go from the document location, if possible.
std::string absolutizePath(SPDocument* doc, const std::string& filename);

// Append 'extension' to 'filename', picking a "_copy_N" variant when the
// target already exists. Returns false when no free name was found.
bool unConflictFilename(SPDocument* doc, std::string& filename, const std::string extension);

// Resolve the directory for 'name' against 'orig', then the document, then the home dir.
std::string prependDirectory(const std::string& name, const std::string& orig, SPDocument* doc = nullptr);

// Filename derived from the object id, placed next to file_entry_text.
std::string filePathFromObject(SPDocument* doc, SPObject* obj, const std::string& file_entry_text);

// Default export path from the document name, or a generated name when unsaved.
std::string defaultFilename(SPDocument* doc, const std::string& filename_entry_text, const std::string& extension);

// Check the directory part of 'filename' exists, creating it if needed.
bool checkOrCreateDirectory(const std::string& filename);

// Render 'area' of the active document to a temporary PNG and run 'extension' on it.
// 'filename' is UTF-8; 'items' (optional) restricts the render to those items.
bool exportRaster(const Geom::Rect& area, const unsigned long int& width, const unsigned long int& height,
                  const float& dpi, const Inkscape::Colors::Color& bgcolor, const Glib::ustring& filename,
                  bool overwrite, unsigned (*callback)(float, void*), void* data,
                  Inkscape::Extension::Output* extension, std::vector<const SPItem*>* items = nullptr);

// Export 'doc' (a copy the callee may modify) through 'extension'.
// 'filename' is UTF-8. The page variant fits the document to 'area' first;
// the items/pages variants crop the copy to those objects and pages.
bool exportVector(Inkscape::Extension::Output* extension, SPDocument* doc, const Glib::ustring& filename,
                  bool overwrite, const Geom::Rect& area);
bool exportVector(Inkscape::Extension::Output* extension, SPDocument* doc, const Glib::ustring& filename,
                  bool overwrite, const std::vector<const SPItem*>& items, const SPPage* page);
bool exportVector(Inkscape::Extension::Output* extension, SPDocument* doc, const Glib::ustring& filename,
                  bool overwrite, const std::vector<const SPItem*>& items, const std::vector<const SPPage*>& pages);

// Persisted export background color on a document object
void set_export_bg_color(SPObject* object, const Inkscape::Colors::Color& color);
Inkscape::Colors::Color get_export_bg_color(SPObject* object, const Inkscape::Colors::Color& default_color);

// One output requested by a UI row: extension + raster DPI.
struct ExportJob {
    Inkscape::Extension::Output* extension = nullptr;
    int dpi = 96;
};

// Export the desktop's current selection once per job. The output path is
// built from the selected object's label (or "Selection"), placed next to the
// document or in the temp dir, and never overwrites an existing file.
// Returns the resolved paths of the files written; the first failure stops
// processing and reports the error on the notification bar.
std::vector<std::string> exportSelection(SPDesktop* desktop, const std::vector<ExportJob>& jobs);

} // namespace Linea::IO

#endif // LINEA_IO_EXPORT_HELPERS_H
