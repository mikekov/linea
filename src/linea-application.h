// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * LineaApplication — document and desktop management singleton for Linea.
 *
 * Owns all open documents (via unique_ptr) and their associated desktops.
 * Also owns the LineaWindow instances and tracks the current active context
 * (document, desktop, selection, window).
 *
 * The Gio/Gtk application lifecycle and CLI/export machinery remain in
 * InkscapeApplication and are not the concern of this class.
 */

#ifndef LINEA_APPLICATION_H
#define LINEA_APPLICATION_H

#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include <glibmm/refptr.h>
#include <QSettings>
#include <QObject>

#include "util/smart_ptr_keys.h"
#include "io/file-error.h"
#include "document.h"
#include "desktop.h"

class LineaWindow;

namespace Inkscape {
class Selection;
} // namespace Inkscape

namespace Gio {
class File;
} // namespace Gio

/**
 * Singleton that owns documents, desktops, and windows for the Linea application.
 *
 * Lifecycle of owned objects:
 *   - Documents:  unique_ptr keys in _documents map.
 *   - Desktops:   unique_ptr values per document in _documents map.
 *   - Windows:    unique_ptr in _windows vector.
 *
 * Delegates raw-pointer tracking (for legacy INKSCAPE macro consumers) to
 * Inkscape::Application via add_document / remove_document / add_desktop /
 * remove_desktop / activate_desktop.
 */
class LineaApplication : public QObject {
    Q_OBJECT
public:
    /// Return the singleton instance. Must have been created via create() first.
    static LineaApplication& instance();
    static bool exists();

    /// Create the singleton. Called once from main (or InkscapeApplication::on_startup).
    static LineaApplication& create();

    // Non-copyable, non-movable singleton.
    LineaApplication(const LineaApplication&) = delete;
    LineaApplication& operator=(const LineaApplication&) = delete;

    // ------------------------------------------------------------------ //
    // Active context
    // These are updated when focus moves between windows/desktops and are
    // used by action handlers that need to know which document to operate on.
    // ------------------------------------------------------------------ //

    SPDocument* get_active_document() const { return _active_document; }
    void set_active_document(SPDocument* document) { _active_document = document; }

    Inkscape::Selection* get_active_selection() const { return _active_desktop ? _active_desktop->getSelection() : nullptr; }
    // void set_active_selection(Inkscape::Selection* sel) { _active_selection = sel; }

    SPDesktop* get_active_desktop() const { return _active_desktop; }
    void set_active_desktop(SPDesktop* desktop);

    LineaWindow* get_active_window() const { return _active_window; }
    void set_active_window(LineaWindow* window) { _active_window = window; }

    // ------------------------------------------------------------------ //
    // Document management
    // ------------------------------------------------------------------ //

    /// Take ownership of an already-constructed document and register it.
    SPDocument* document_add(std::unique_ptr<SPDocument> document);

    /// Create a new document from a template (or default template if empty).
    SPDocument* document_new(const std::string& template_filename = {});

    /// Open a document from a file, add it, and return it.
    std::pair<SPDocument*, Linea::IO::FileError> document_open(const Glib::RefPtr<Gio::File>& file);

    /// Open a document from an in-memory SVG buffer.
    SPDocument* document_open(std::span<const char> buffer);

    /// Swap the document shown by a desktop (does not delete the old document).
    bool document_swap(SPDesktop* desktop, SPDocument* document);

    /// Reopen the saved version of a document and swap it into all windows.
    bool document_revert(SPDocument* document);

    /// Remove a document from the app and destroy it. No desktop must be attached.
    void document_close(SPDocument* document);

    /// Apply GUI-required fixes to a newly opened document (broken links, DPI, LPE).
    void document_fix(SPDesktop* desktop);

    /// Return raw pointers to all currently open documents.
    std::vector<SPDocument*> get_documents() const;

    // ------------------------------------------------------------------ //
    // Desktop management
    // ------------------------------------------------------------------ //

    /// Open a new desktop (tab/window) for an existing document.
    SPDesktop* desktopOpen(SPDocument* document, bool new_window = false);

    /// Close a desktop without deleting its document.
    void desktopClose(SPDesktop* desktop);

    /// Close the currently active desktop.
    void desktopCloseActive();

    // ------------------------------------------------------------------ //
    // Window management
    // ------------------------------------------------------------------ //

    /// Called by LineaWindow when it is about to be destroyed.
    void windowClose(LineaWindow* window);

    /// High-level: create a desktop for a document, reusing the active window if replace=true.
    SPDesktop* createDesktop(SPDocument* document, bool replace, bool new_window = false);

    /// Create an empty main window with no document loaded (shows the welcome page).
    LineaWindow* createWindow();

    /// High-level: open a file and show it in a window.
    void openDocument(const Glib::RefPtr<Gio::File>& file);

    /// Create a new document; use given template or default one (if 0 is specified)
    void createNewDocument(int templateIndex = 0);

    /// Destroy a desktop and its document, checking for unsaved data.
    /// If keep_alive=true and this is the last window, replaces with a new document.
    bool destroyDesktop(SPDesktop* desktop, bool keep_alive = false);

    /// Move a desktop out of its current window into a new standalone window.
    void detachDesktopToNewWindow(SPDesktop* desktop);

    /// Close all windows and documents (used during shutdown). Returns false if aborted.
    bool destroy_all();

    /// Unconditional teardown: release all windows, desktops, and documents without
    /// showing any dialogs. Must be called while QApplication is still alive.
    void shutdown();

    /// Number of desktops currently open (across all documents and windows).
    int get_number_of_windows() const;

    /// App-wide settings
    QSettings& settings() { return *_settings; }

private:
    // Private ctor/dtor — use create() / instance().
    LineaApplication();
    ~LineaApplication() = default;

    // class ConstructibleApplication;
    static LineaApplication* _instance;

    // Documents are owned by this application.
    // Each document maps to the desktops (tabs/windows) currently showing it.
    std::unordered_map<std::unique_ptr<SPDocument>, std::vector<std::unique_ptr<SPDesktop>>,
                       TransparentPtrHash<SPDocument>, TransparentPtrEqual<SPDocument>>
        _documents;

    std::vector<std::unique_ptr<LineaWindow>> _windows;

    QSettings* _settings = nullptr;

    // Active context (raw, non-owning pointers).
    SPDocument* _active_document = nullptr;
    SPDesktop* _active_desktop = nullptr;
    LineaWindow* _active_window = nullptr;
};

/// Convenience macro — analogous to INKSCAPE for Inkscape::Application.
#define LINEA_APP (LineaApplication::instance())

#endif // LINEA_APPLICATION_H
