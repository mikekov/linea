// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Qt replacements for GTK file-chooser helpers (src/ui/dialog/choose-file.h).
 *
 * Only the subset needed by the Qt/Linea build is provided here.
 */

#ifndef LINEA_QT_CHOOSE_FILE_H
#define LINEA_QT_CHOOSE_FILE_H

#include <string>
#include <vector>
#include <giomm/file.h>
#include <glibmm/refptr.h>
#include <glibmm/ustring.h>

class LineaWindow;
class QString;

namespace Inkscape {

namespace UI::Dialog {

/**
 * Build a Qt file-type filter string from all registered Inkscape output
 * extensions, mirroring the GTK create_export_filters().
 *
 * @param for_save  If true, exclude raster formats (save dialogs cannot
 *                  handle them).
 * @return          Qt filter string suitable for QFileDialog, e.g.
 *                  "All Files (*);;All Supported Files (*.svg …);;SVG (.svg);;…".
 */
[[nodiscard]] QString create_export_filters(bool for_save = false);

} // namespace UI::Dialog

/**
 * Show a modal "open file(s)" dialog filtered to all formats supported by
 * Inkscape's input extensions (SVG, PNG, JPEG, …).
 *
 * Mirrors the GTK-based Inkscape::choose_file_open_images() signature but
 * takes a LineaWindow* instead of a Gtk::Window*.
 *
 * @param title       Dialog title.
 * @param parent      Parent window (may be nullptr).
 * @param pref_path   Inkscape prefs key used to persist the last-used directory.
 * @param accept      Label for the accept button (e.g. "Open" or "Import").
 * @return            List of selected files (empty if cancelled).
 */
[[nodiscard]] std::vector<Glib::RefPtr<Gio::File>> choose_file_open_images(
    const Glib::ustring& title,
    LineaWindow* parent,
    const std::string& pref_path,
    const Glib::ustring& accept = {}
);

/**
 * Show a modal "save file" dialog.
 *
 * @param title           Dialog title.
 * @param parent          Parent window (may be nullptr).
 * @param filters         Qt file-type filter string (e.g. from create_export_filters()).
 * @param file_name       Initial file name.
 * @param current_folder  Initial folder; updated to the parent of the selected file.
 * @return                Selected file, or empty if cancelled.
 */
[[nodiscard]] Glib::RefPtr<Gio::File> choose_file_save(
    const Glib::ustring& title,
    LineaWindow* parent,
    const QString& filters,
    const std::string& file_name,
    std::string& current_folder);

/**
 * Show a modal "save file" dialog with a single MIME-type filter.
 *
 * @param title           Dialog title.
 * @param parent          Parent window (may be nullptr).
 * @param mime_type       MIME type to derive a file filter from.
 * @param file_name       Initial file name.
 * @param current_folder  Initial folder; updated to the parent of the selected file.
 * @return                Selected file, or empty if cancelled.
 */
[[nodiscard]] Glib::RefPtr<Gio::File> choose_file_save(
    const Glib::ustring& title,
    LineaWindow* parent,
    const Glib::ustring& mime_type,
    const std::string& file_name,
    std::string& current_folder);

} // namespace Inkscape

#endif // LINEA_QT_CHOOSE_FILE_H
