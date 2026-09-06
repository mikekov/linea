// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Qt implementation of Inkscape::choose_file_open_images().
 */

#include "choose-file.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>
#include <QString>
#include <QStringList>
#include <algorithm>
#include <giomm/file.h>
#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "extension/db.h"
#include "extension/input.h"
#include "extension/output.h"
#include "linea-window.h"
#include "preferences.h"

namespace Inkscape {

namespace {

/**
 * Build a Qt filter string from all registered Inkscape input extensions.
 *
 * Produces:   "All Supported Files (*.svg *.png …);;SVG (*.svg);;PNG (*.png);;…"
 * mirroring what create_open_filters() does for GTK.
 */
QString build_qt_open_filters() {
    Inkscape::Extension::DB::InputList ext_list;
    Inkscape::Extension::db.get_input_list(ext_list);

    QStringList all_patterns;
    QStringList per_format;

    for (auto imod : ext_list) {
        const char* raw = imod->get_extension(); // e.g. ".svg"
        if (!raw || !raw[0]) continue;
        // Qt filter patterns don't include the leading dot.
        QString pattern = QStringLiteral("*.") + QString::fromUtf8(raw + 1).toLower();

        all_patterns << pattern;

        // "name" here has a desired format already: "SVG (*.svg)" or "PNG (*.png)"
        QString name = QString::fromUtf8(imod->get_filetypename(true));
        per_format << name;
    }

    if (all_patterns.isEmpty()) {
        return QObject::tr("All Files (*)");
    }

    QString combined = QStringLiteral("All Supported Files (%1)").arg(all_patterns.join(' '));
    // QString combined = QStringLiteral("All Supported Files");
    combined += QStringLiteral(";;All Files (*)");
    combined += QStringLiteral(";;") + per_format.join(QStringLiteral(";;"));
    return combined;
}

} // namespace

namespace UI::Dialog {

/**
 * Build a Qt filter string from all registered Inkscape output extensions.
 *
 * Mirrors create_export_filters() in src/ui/dialog/choose-file-utils.cpp.
 * When @a for_save is true, raster formats are excluded (save dialogs cannot
 * handle them).
 */
QString create_export_filters(bool for_save) {
    Inkscape::Extension::DB::OutputList ext_list;
    Inkscape::Extension::db.get_output_list(ext_list);

    QStringList all_patterns;
    QStringList per_format;
    std::vector<std::string> seen_extensions;

    for (auto* omod : ext_list) {
        // Save dialogs cannot handle raster images.
        if (for_save && omod->is_raster()) {
            continue;
        }

        const char* raw = omod->get_extension(); // e.g. ".svg"
        if (!raw || !raw[0]) continue;
        std::string extension(raw + 1);
        std::string extension_lower = extension;
        // De-duplicate by lower-case extension.
        if (std::find(seen_extensions.begin(), seen_extensions.end(), extension_lower) != seen_extensions.end()) {
            continue;
        }
        seen_extensions.push_back(extension_lower);

        QString pattern = QStringLiteral("*.") + QString::fromStdString(extension_lower);
        all_patterns << pattern;

        QString name = QString::fromUtf8(omod->get_filetypename(true));
        // Use the same simplified names as the GTK version.
        if (extension_lower == "svg") {
            name = QStringLiteral("SVG (.svg)");
        } else if (extension_lower == "svgz") {
            name = QObject::tr("Compressed SVG (.svgz)");
        } else if (extension_lower == "dxf") {
            name = QStringLiteral("DXF (.dxf)");
        } else if (extension_lower == "zip") {
            name = QStringLiteral("ZIP (.zip)");
        } else if (extension_lower == "pdf") {
            name = QStringLiteral("PDF (.pdf)");
        } else if (extension_lower == "png") {
            name = QStringLiteral("PNG (.png)");
        }

        per_format << name;
    }

    if (all_patterns.isEmpty()) {
        return QObject::tr("All Files (*)");
    }

    QString combined = QStringLiteral("All Files (*)");
    combined += QStringLiteral(";;All Supported Files (%1)").arg(all_patterns.join(' '));
    // combined += QStringLiteral(";;All Supported Files");
    combined += QStringLiteral(";;") + per_format.join(QStringLiteral(";;"));
    return combined;
}

} // namespace UI::Dialog

std::vector<Glib::RefPtr<Gio::File>> choose_file_open_images(const Glib::ustring& title, LineaWindow* parent,
                                                             const std::string& pref_path,
                                                             const Glib::ustring& accept) {
    // Resolve starting directory: check prefs, fall back to Documents then home.
    std::string start_dir = Inkscape::Preferences::get()->getString(pref_path);
    if (!start_dir.empty() && !Glib::file_test(start_dir, Glib::FileTest::EXISTS)) {
        start_dir.clear();
    }
    if (start_dir.empty()) {
        start_dir = Glib::get_user_special_dir(Glib::UserDirectory::DOCUMENTS);
    }
    if (start_dir.empty()) {
        start_dir = Glib::get_home_dir();
    }

    QStringList paths = QFileDialog::getOpenFileNames(parent, QString::fromUtf8(title.c_str()),
                                                      QString::fromStdString(start_dir), build_qt_open_filters());

    if (paths.isEmpty()) {
        return {};
    }

    // Persist the directory for single-file selections (matches GTK behaviour).
    if (paths.size() == 1) {
        std::string dir = QFileInfo(paths.first()).dir().absolutePath().toStdString();
        Inkscape::Preferences::get()->setString(pref_path, dir);
    }

    std::vector<Glib::RefPtr<Gio::File>> files;
    files.reserve(paths.size());
    for (const auto& p : paths) {
        files.push_back(Gio::File::create_for_path(p.toStdString()));
    }
    return files;
}

namespace {

/// Return @a dir if it exists, otherwise the user's home directory.
std::string ensure_dir(const std::string& dir) {
    if (!dir.empty() && Glib::file_test(dir, Glib::FileTest::EXISTS)) {
        return dir;
    }
    return Glib::get_home_dir();
}

/// Convert a MIME type to a Qt file filter string, e.g. "image/png" → "PNG (*.png)".
QString mime_type_to_filter(const Glib::ustring& mime_type) {
    if (mime_type.empty()) {
        return QObject::tr("All Files (*)");
    }

    QMimeDatabase db;
    const auto mime = db.mimeTypeForName(QString::fromUtf8(mime_type.c_str()));
    if (!mime.isValid()) {
        return QObject::tr("All Files (*)");
    }

    QStringList patterns;
    for (const auto& suffix : mime.suffixes()) {
        patterns << QStringLiteral("*.%1").arg(suffix);
    }
    if (patterns.isEmpty()) {
        return QObject::tr("All Files (*)");
    }

    return QStringLiteral("%1 (%2)").arg(mime.comment(), patterns.join(' '));
}

} // namespace

Glib::RefPtr<Gio::File> choose_file_save(const Glib::ustring& title, LineaWindow* parent, const QString& filters,
                                         const std::string& file_name, std::string& current_folder) {
    if (!parent) {
        return {};
    }

    const std::string start_dir = ensure_dir(current_folder);
    const QDir dir(QString::fromStdString(start_dir));
    const QString initial_path = dir.filePath(QString::fromStdString(file_name));

    const QString path = QFileDialog::getSaveFileName(parent, QString::fromUtf8(title.c_str()), initial_path,
                                                      filters.isEmpty() ? QObject::tr("All Files (*)") : filters);

    if (path.isEmpty()) {
        return {};
    }

    current_folder = QFileInfo(path).dir().absolutePath().toStdString();
    return Gio::File::create_for_path(path.toStdString());
}

Glib::RefPtr<Gio::File> choose_file_save(const Glib::ustring& title, LineaWindow* parent,
                                         const Glib::ustring& mime_type, const std::string& file_name,
                                         std::string& current_folder) {
    return choose_file_save(title, parent, mime_type_to_filter(mime_type), file_name, current_folder);
}

} // namespace Inkscape
