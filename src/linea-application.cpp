// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * LineaApplication — document and desktop management singleton for Linea.
 */

#include "linea-application.h"

#include <cassert>
#include <iostream>
#include <numeric>
#include <giomm/file.h>

#include "desktop.h"
#include "document.h"
#include "file.h"               // ink_file_new
#include "inkscape.h"           // Inkscape::Application (INKSCAPE macro)
#include "io/file.h"            // ink_file_open buffer overload
#include "io/recent-files.h"
#include "io/resource.h"        // TEMPLATES
#include "linea-window.h"
#include "object/sp-root.h"
#include "ui/desktop/document-check.h" // document_check_for_data_loss
#include "ui/desktop/desktop-widget.h"
#include "ui/util.h"

// ---------------------------------------------------------------------------

LineaApplication* LineaApplication::_instance = nullptr;

LineaApplication& LineaApplication::create() {
    if (_instance) {
        std::cerr << "LineaApplication::create: instance already exists" << std::endl;
        return *_instance;
    }
    static LineaApplication app;
    _instance = &app;
    return *_instance;
}

bool LineaApplication::exists() {
    return _instance != nullptr;
}

LineaApplication& LineaApplication::instance() {
    assert(_instance);
    return *_instance;
}

LineaApplication::LineaApplication() {
    _settings = new QSettings(QSettings::UserScope, "Linea", "LineaDraw", this);
}

// ---------------------------------------------------------------------------
// Active context
// ---------------------------------------------------------------------------

void LineaApplication::set_active_desktop(SPDesktop* desktop) {
    _active_desktop = desktop;
    if (desktop) {
        INKSCAPE.activate_desktop(desktop);
        // Don't coalesce undo events across leaving then returning to a desktop.
        desktop->getDocument()->resetKey();
    }
}

// ---------------------------------------------------------------------------
// Document management
// ---------------------------------------------------------------------------

void LineaApplication::createNewDocument(int templateIndex) {
    UI::OverrideCursor wait(Qt::WaitCursor);

    //TODO: configurable templates -------
    auto fname = "default.svg";
    if (templateIndex == 2) {
        fname = "default-wide.svg";
    }
    else if (templateIndex == 3) {
        fname = "default-a4.svg";
    }
    else if (templateIndex == 4) {
        fname = "default-us-letter.svg";
    }
    auto def = Inkscape::IO::Resource::get_filename(Inkscape::IO::Resource::TEMPLATES, fname, true);

    SPDesktop* desktop = nullptr;
    auto document = document_new(def);
    if (document) {
        desktop = desktopOpen(document);
    } else {
        std::cerr << "LineaApplication::createNewDocument: Failed to open default document!" << std::endl;
    }

    _active_document = document;
    _active_window = desktop ? desktop->getLineaWindow() : nullptr;
}

SPDocument* LineaApplication::document_add(std::unique_ptr<SPDocument> document) {
    assert(document);
    auto [it, inserted] = _documents.try_emplace(std::move(document));
    assert(inserted);
    INKSCAPE.add_document(it->first.get());
    return it->first.get();
}

// New document from template (or default template if empty string).
SPDocument* LineaApplication::document_new(const std::string& template_filename) {
    if (template_filename.empty()) {
        auto def = Inkscape::IO::Resource::get_filename(Inkscape::IO::Resource::TEMPLATES, "default.svg", true);
        if (!def.empty()) {
            return document_new(def);
        }
    }

    auto doc_uniq = ink_file_new(template_filename);
    if (!doc_uniq) {
        std::cerr << "LineaApplication::document_new: failed to open new document!" << std::endl;
        return nullptr;
    }

    auto doc = document_add(std::move(doc_uniq));

    // Set viewBox if it doesn't exist.
    if (!doc->getRoot()->viewBox_set) {
        doc->setViewBox();
    }

    return doc;
}

std::pair<SPDocument*, Linea::IO::FileError> LineaApplication::document_open(const Glib::RefPtr<Gio::File>& file) {
    auto [document, error] = ink_file_open(file);
    if (error.cancelled()) {
        return {nullptr, error};
    }
    if (!document) {
        std::cerr << "LineaApplication::document_open: Failed to open: " << file->get_parse_name().raw() << std::endl;
        return {nullptr, error};
    }

    document->setVirgin(false); // Prevents replacing document in same window during file open.

    auto path = file->get_path();
    // Opening crash files or auto-save files, we can link them back using the
    // recent files manager to get the original context for the file.
    if (auto original = Linea::IO::openAsInkscapeRecentOriginalFile(path)) {
        document->setModifiedSinceSave(true);
        document->setModifiedSinceAutoSaveFalse();
        document->setDocumentFilename(original->empty() ? nullptr : original->c_str());
    } else {
        auto name = document->getDocumentName();
        Linea::IO::addInkscapeRecentSvg(path, name ? name : "");
    }

    return {document_add(std::move(document)), Linea::IO::FileError()};
}

SPDocument* LineaApplication::document_open(std::span<const char> buffer) {
    auto document = ink_file_open(buffer);
    if (!document) {
        std::cerr << "LineaApplication::document_open: Failed to open memory document." << std::endl;
        return nullptr;
    }

    document->setVirgin(false);
    return document_add(std::move(document));
}

/**
 * Swap out one document for another in a desktop tab.
 * Does not delete the old document — the caller is responsible for it.
 */
bool LineaApplication::document_swap(SPDesktop* desktop, SPDocument* document) {
    if (!document || !desktop) {
        std::cerr << "LineaApplication::document_swap: Missing desktop or document!" << std::endl;
        return false;
    }

    auto old_document = desktop->getDocument();
    desktop->change_document(document);

    // Move the desktop from the old document's slot to the new document's slot.
    auto doc_it = _documents.find(old_document);
    if (doc_it == _documents.end()) {
        std::cerr << "LineaApplication::document_swap: Old document not in map!" << std::endl;
        return false;
    }

    auto dt_it =
        std::find_if(doc_it->second.begin(), doc_it->second.end(), [=](auto& dt) { return dt.get() == desktop; });
    if (dt_it == doc_it->second.end()) {
        std::cerr << "LineaApplication::document_swap: Desktop not found!" << std::endl;
        return false;
    }

    auto dt_uniq = std::move(*dt_it);
    doc_it->second.erase(dt_it);

    auto new_doc_it = _documents.find(document);
    if (new_doc_it == _documents.end()) {
        std::cerr << "LineaApplication::document_swap: New document not in map!" << std::endl;
        return false;
    }

    new_doc_it->second.push_back(std::move(dt_uniq));

    _active_document = document;
    return true;
}

/**
 * Revert a document: open the saved version from disk and swap it into every
 * desktop that was showing the old document.
 */
bool LineaApplication::document_revert(SPDocument* document) {
    const char* path = document->getDocumentFilename();
    if (!path) {
        std::cerr << "LineaApplication::document_revert: Document never saved, cannot revert." << std::endl;
        return false;
    }

    auto file = Gio::File::create_for_path(path);
    auto [new_document, error] = document_open(file);
    if (!new_document) {
        if (!error.cancelled()) {
            std::cerr << "LineaApplication::document_revert: Cannot open saved document!" << std::endl;
        }
        return false;
    }

    document->setVirgin(true); // Allow overwriting.

    auto it = _documents.find(document);
    if (it == _documents.end()) {
        std::cerr << "LineaApplication::document_revert: Document not found!" << std::endl;
        return false;
    }

    // Collect the desktops attached to the old document before swapping.
    std::vector<SPDesktop*> desktops;
    for (const auto& dt : it->second) {
        desktops.push_back(dt.get());
    }

    for (auto dt : desktops) {
        double zoom = dt->current_zoom();
        Geom::Point c = dt->current_center();

        if (document_swap(dt, new_document)) {
            dt->zoom_absolute(c, zoom, false);
            // QT TODO: sp_file_fix_lpe(dt->getDocument())
        } else {
            std::cerr << "LineaApplication::document_revert: Revert failed!" << std::endl;
        }
    }

    document_close(document);
    return true;
}

/**
 * Remove a document from the app and destroy it.
 * No desktops must still be attached to it.
 */
void LineaApplication::document_close(SPDocument* document) {
    if (!document) {
        std::cerr << "LineaApplication::document_close: No document!" << std::endl;
        return;
    }

    auto it = _documents.find(document);
    if (it == _documents.end()) {
        std::cerr << "LineaApplication::document_close: Document not registered." << std::endl;
        return;
    }

    if (!it->second.empty()) {
        std::cerr << "LineaApplication::document_close: Desktop vector not empty!" << std::endl;
        return;
    }

    if (_active_document == document) {
        _active_document = nullptr;
    }

    INKSCAPE.remove_document(it->first.get());
    _documents.erase(it);
}

/**
 * Apply GUI-required fixes to a document that has just been opened.
 *
 * QT TODO: fixBrokenLinks pulls in GtkRecentManager and requires an active
 * Gtk::Application.  sp_file_convert_dpi opens a Gtk::Dialog.  Neither is
 * usable in the Qt-only path yet.  Re-enable once Qt-native equivalents exist.
 */
void LineaApplication::document_fix([[maybe_unused]] SPDesktop *desktop)
{
    // QT TODO: Inkscape::fixBrokenLinks(document)  — needs GtkRecentManager
    // QT TODO: sp_file_convert_dpi(document)       — needs Gtk::Dialog
    // QT TODO: sp_file_fix_lpe(document)           — safe, enable when above are resolved
}

std::vector<SPDocument*> LineaApplication::get_documents() const {
    std::vector<SPDocument*> result;
    result.reserve(_documents.size());
    for (const auto& [doc, _] : _documents) {
        result.push_back(doc.get());
    }
    return result;
}

// ---------------------------------------------------------------------------
// Desktop management
// ---------------------------------------------------------------------------

/**
 * Open a new desktop for an existing document.
 * If an active window exists and new_window is false, the desktop is added as
 * a tab to that window; otherwise a new LineaWindow is created.
 */
SPDesktop* LineaApplication::desktopOpen(SPDocument* document, bool new_window) {
    assert(document);

    const auto doc_it = _documents.find(document);
    if (doc_it == _documents.end()) {
        std::cerr << "LineaApplication::desktopOpen: Document not in map!" << std::endl;
        return nullptr;
    }

    auto desktop = doc_it->second.emplace_back(std::make_unique<SPDesktop>(document->getNamedView())).get();
    INKSCAPE.add_desktop(desktop);

    if (_active_window && !new_window) {
        _active_window->getDesktopWidget()->addDesktop(desktop);
    } else {
        // Set active context before constructing the window so that any
        // callbacks triggered during construction see a consistent state.
        _active_document  = document;
        _active_desktop   = desktop;
        // _active_selection = desktop->getSelection();

        auto win = createWindow();
        win->addDesktop(desktop);
    }

    document_fix(desktop);
    return desktop;
}

/**
 * Close a desktop without deleting its document.
 */
void LineaApplication::desktopClose(SPDesktop* desktop) {
    if (!desktop) {
        std::cerr << "LineaApplication::desktopClose: No desktop!" << std::endl;
        return;
    }

    auto document = desktop->getDocument();
    if (!document) {
        std::cerr << "LineaApplication::desktopClose: Desktop has no document!" << std::endl;
        return;
    }

    auto doc_it = _documents.find(document);
    if (doc_it == _documents.end()) {
        std::cerr << "LineaApplication::desktopClose: Document not in map!" << std::endl;
        return;
    }

    auto dt_it =
        std::find_if(doc_it->second.begin(), doc_it->second.end(), [=](auto& dt) { return dt.get() == desktop; });
    if (dt_it == doc_it->second.end()) {
        std::cerr << "LineaApplication::desktopClose: Desktop not found!" << std::endl;
        return;
    }

    auto win = desktop->getLineaWindow();
    if (!win || !win->getDesktopWidget()) {
        std::cerr << "LineaApplication::desktopClose: Desktop has no window!" << std::endl;
        return;
    }

    win->getDesktopWidget()->removeDesktop(desktop);

    INKSCAPE.remove_desktop(desktop); // Clears selection and event context.
    doc_it->second.erase(dt_it);      // Triggers SPDesktop destructor.
}

void LineaApplication::desktopCloseActive() {
    if (!_active_desktop) {
        std::cerr << "LineaApplication::desktopCloseActive: no active desktop!" << std::endl;
        return;
    }
    desktopClose(_active_desktop);
}

// ---------------------------------------------------------------------------
// Window management
// ---------------------------------------------------------------------------

/**
 * Create an empty main window with no document loaded.
 * The window shows the welcome page until a desktop is added to it.
 */
LineaWindow* LineaApplication::createWindow() {
    auto win = _windows.emplace_back(std::make_unique<LineaWindow>(_settings)).get();
    _active_window = win;

    // sp_namedview_window_from_document is GTK-based window sizing logic.
    // LineaWindow::present() handles geometry via Qt's restoreGeometry instead.
    win->present();
    return win;
}


/**
 * Open a file and show it in a window.
 */
void LineaApplication::openDocument(const Glib::RefPtr<Gio::File>& file) {
    if (!file) {
        std::cerr << "LineaApplication::createWindow: No file specified!" << std::endl;
        return;
    }

    UI::OverrideCursor wait(Qt::WaitCursor);

    auto [document, error] = document_open(file);
    if (!document) {
        if (!error.cancelled()) {
            //todo: notification bar
            if (_active_window) {
                _active_window->getDesktopWidget()->showError(
                    tr("Cannot open file"), QString::fromStdString(file->get_basename()),
                    error.error());
            }
            std::cerr << "LineaApplication::createWindow: Failed to load: " << file->get_parse_name().raw()
                      << std::endl;
        }
        return;
    }

    auto docname = document->getDocumentName();
    Linea::IO::addInkscapeRecentSvg(file->get_path(), docname ? docname : "");

    bool replace = _active_document && _active_document->getVirgin();
    auto desktop = createDesktop(document, replace);
    document_fix(desktop);

    _active_document = document;
    _active_window = desktop ? desktop->getLineaWindow() : nullptr;
}

void LineaApplication::windowClose(LineaWindow* window) {
    if (window == _active_window) {
        _active_window = nullptr;
    }

    auto win_it = std::find_if(_windows.begin(), _windows.end(), [=](auto& w) { return w.get() == window; });
    if (win_it != _windows.end()) {
        _windows.erase(win_it);
    }
}

/**
 * Create a desktop for document, optionally reusing the active desktop (replace=true).
 */
SPDesktop* LineaApplication::createDesktop(SPDocument* document, bool replace, bool new_window) {
    assert(document);

    auto old_document = _active_document;
    auto desktop = _active_desktop;

    if (replace && old_document && desktop) {
        document_swap(desktop, document);

        // Close the old document if it has no more desktops.
        auto it = _documents.find(old_document);
        if (it != _documents.end() && it->second.empty()) {
            document_close(old_document);
        }
    } else {
        desktop = desktopOpen(document, new_window);
    }

    return desktop;
}

/**
 * Destroy a desktop and (if no other desktops remain) its document.
 * Returns false if aborted due to unsaved data.
 */
bool LineaApplication::destroyDesktop(SPDesktop* desktop, [[maybe_unused]] bool keep_alive) {
    if (!desktop) {
        std::cerr << "LineaApplication::destroyDesktop: No desktop!" << std::endl;
        return false;
    }

    auto document = desktop->getDocument();
    if (!document) {
        std::cerr << "LineaApplication::destroyDesktop: desktop has no document!" << std::endl;
        return false;
    }

    auto it = _documents.find(document);
    if (it == _documents.end()) {
        std::cerr << "LineaApplication::destroyDesktop: Could not find document!" << std::endl;
        return false;
    }

    // If this is the only desktop for the document, check for unsaved data.
    if (it->second.size() == 1) {
        if (document_check_for_data_loss(desktop)) {
            return false; // User aborted.
        }
    }

    if (get_number_of_windows() == 1 && keep_alive) {
        // Last desktop: replace with a new blank document instead of closing.
        auto new_document = document_new();
        document_swap(desktop, new_document);
        // desktopClose(desktop);
    } else {
        desktopClose(desktop);
    }

    // Close the document if it no longer has any desktops.
    if (it->second.empty()) {
        document_close(document);
    }

    return true;
}

/**
 * Remove a desktop from its current window and re-open it in a new standalone window.
 */
void LineaApplication::detachDesktopToNewWindow(SPDesktop* desktop) {
    auto old_win = desktop->getLineaWindow();
    old_win->getDesktopWidget()->removeDesktop(desktop);

    // Set active context before constructing the window so that any callbacks
    // triggered during construction see a consistent state.
    _active_document = desktop->getDocument();
    _active_desktop = desktop;

    auto new_win = createWindow();
    new_win->addDesktop(desktop);
}

/**
 * Close all documents and windows. Returns false if aborted.
 */
bool LineaApplication::destroy_all() {
    while (!_documents.empty()) {
        auto& [doc, desktops] = *_documents.begin();
        if (!desktops.empty()) {
            if (!destroyDesktop(desktops.back().get())) {
                return false;
            }
        } else {
            document_close(doc.get());
        }
    }
    return true;
}

/**
 * Unconditional teardown while QApplication is still alive.
 * Closes all desktops (removing them from their windows) then closes all
 * documents, then drops all window ownership.  No data-loss checks are done —
 * the caller (aboutToQuit) is responsible for ensuring the user has already
 * had the chance to save.
 */
void LineaApplication::shutdown()
{
    // Close every desktop via the normal low-level path so that INKSCAPE's
    // desktop tracking and widget linkage are cleaned up properly.
    for (auto &[doc, desktops] : _documents) {
        // Collect raw pointers first — desktopClose mutates the vector.
        std::vector<SPDesktop *> dts;
        for (auto &dt : desktops) {
            dts.push_back(dt.get());
        }
        for (auto *dt : dts) {
            auto *win = dt->getLineaWindow();
            if (win) {
                win->getDesktopWidget()->removeDesktop(dt);
            }
            INKSCAPE.remove_desktop(dt);
        }
        desktops.clear();
    }

    // Close all documents (unique_ptrs destroyed here).
    for (auto &[doc, _] : _documents) {
        INKSCAPE.remove_document(doc.get());
    }
    _documents.clear();

    // Destroy all windows (Qt widgets) while QApplication is still alive.
    _windows.clear();

    _active_document  = nullptr;
    // _active_selection = nullptr;
    _active_desktop   = nullptr;
    _active_window    = nullptr;
}

int LineaApplication::get_number_of_windows() const {
    return std::accumulate(_documents.begin(), _documents.end(), 0,
                           [](int sum, const auto& v) { return sum + static_cast<int>(v.second.size()); });
}
