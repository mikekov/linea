// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Main window for Linea app.
 */

#ifndef LINEA_MAINWINDOW_H
#define LINEA_MAINWINDOW_H

#include <QCloseEvent>
#include <QKeyEvent>
#include <QMainWindow>
#include <QResizeEvent>
#include <QSettings>
#include <QShowEvent>
#include <giomm/actionmap.h>
#include <giomm/simpleaction.h>
#include <giomm/simpleactiongroup.h>
#include <glibmm/refptr.h>
#include <glibmm/ustring.h>
#include <glibmm/variant.h>
#include <glibmm/varianttype.h>

QT_BEGIN_NAMESPACE
class QToolBar;
class QAction;
QT_END_NAMESPACE

class SPDocument;
class SPDesktop;
class LineaApplication;
class LineaWindow;

namespace Linea::UI {
class SPDesktopWidget;
}

/**
 * Main window hosting the Linea canvas frame and UI.
 */
class LineaWindow : public QMainWindow {
    Q_OBJECT

public:
    /// Create an empty window with no document loaded (shows the welcome page).
    /// Use addDesktop() to load a desktop after construction.
    LineaWindow(QSettings* settings, QWidget* parent = nullptr);
    ~LineaWindow() override;

    /// Add the first (or a subsequent) desktop to this window.
    void addDesktop(SPDesktop* desktop);

    // Document/Desktop accessors (matching LineaWindow interface)
    SPDocument* get_document() const { return _document; }
    SPDesktop* get_desktop() const { return _desktop; }

    Linea::UI::SPDesktopWidget* getDesktopWidget() const { return _desktop_widget; }

    // Window state queries
    bool isFullscreen() const;
    bool isMaximised() const;
    bool isMinimized() const;

    // Window state changes
    void present();
    void toggleFullscreen();
    bool getFullscreen() const;
    void toggleDialogs();
    bool dialogsVisible() const;
    void toggleRulers();
    bool rulersVisible() const;
    void toggleColorPalette();
    bool colorPaletteVisible() const;
    void maximize();
    void fullscreen();
    void change_document(SPDocument* document);
    void setActiveTab(SPDesktop* desktop);

    // Refresh the window title from the active document's name and save state
    void updateTitle();

    // Window geometry access
    QScreen* get_surface() const;
    int get_width() const;
    int get_height() const;

    // Action listing
    std::vector<Glib::ustring> list_actions() const;

    // Action map — delegates to _action_group (a Gio::SimpleActionGroup)
    Glib::RefPtr<Gio::ActionMap> getActionMap() { return std::static_pointer_cast<Gio::ActionMap>(_action_group); }
    Glib::RefPtr<const Gio::ActionMap> getActionMap() const {
        return std::static_pointer_cast<const Gio::ActionMap>(_action_group);
    }

    Glib::RefPtr<Gio::SimpleAction> add_action(const Glib::ustring& name, const Gio::ActionMap::ActivateSlot& slot) {
        return _action_group->add_action(name, slot);
    }
    Glib::RefPtr<Gio::SimpleAction> add_action_with_parameter(const Glib::ustring& name,
                                                              const Glib::VariantType& parameter_type,
                                                              const Gio::ActionMap::ActivateWithParameterSlot& slot) {
        return _action_group->add_action_with_parameter(name, parameter_type, slot);
    }
    Glib::RefPtr<Gio::SimpleAction> add_action_bool(const Glib::ustring& name, bool state = false) {
        return _action_group->add_action_bool(name, state);
    }
    Glib::RefPtr<Gio::SimpleAction> add_action_bool(const Glib::ustring& name, const Gio::ActionMap::ActivateSlot& slot,
                                                    bool state = false) {
        return _action_group->add_action_bool(name, slot, state);
    }
    Glib::RefPtr<Gio::SimpleAction> add_action_radio_string(const Glib::ustring& name, const Glib::ustring& state) {
        return _action_group->add_action_radio_string(name, state);
    }
    Glib::RefPtr<Gio::SimpleAction> add_action_radio_string(const Glib::ustring& name,
                                                            const Gio::ActionMap::ActivateWithStringParameterSlot& slot,
                                                            const Glib::ustring& state) {
        return _action_group->add_action_radio_string(name, slot, state);
    }
    Glib::RefPtr<Gio::SimpleAction> add_action_radio_integer(const Glib::ustring& name, gint32 state) {
        return _action_group->add_action_radio_integer(name, state);
    }
    Glib::RefPtr<Gio::SimpleAction> add_action_radio_integer(const Glib::ustring& name,
                                                             const Gio::ActionMap::ActivateWithIntParameterSlot& slot,
                                                             gint32 state) {
        return _action_group->add_action_radio_integer(name, slot, state);
    }
    Glib::RefPtr<Gio::Action> lookup_action(const Glib::ustring& name) { return _action_group->lookup_action(name); }
    Glib::RefPtr<const Gio::Action> lookup_action(const Glib::ustring& name) const {
        return _action_group->lookup_action(name);
    }
    bool has_action(const Glib::ustring& name) const {
        return std::static_pointer_cast<Gio::ActionGroup>(_action_group)->has_action(name);
    }
    void activate_action(const Glib::ustring& name, const Glib::VariantBase& parameter) {
        std::static_pointer_cast<Gio::ActionGroup>(_action_group)->activate_action(name, parameter);
    }
    void activate_action(const Glib::ustring& name) {
        std::static_pointer_cast<Gio::ActionGroup>(_action_group)->activate_action(name);
    }

    // public Q_SLOTS:

protected:
    void closeEvent(QCloseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private Q_SLOTS:
    void onFocusChanged();
    void onWindowStateChanged();

private:
    void setupUI();
    void createActions();
    void createToolBar();
    void createMenu();
    void add_document_actions();
    void update_dialogs();
    void saveSettings();
    void restoreSettings();

    // Application instance
    LineaApplication* _app = nullptr;

    // Document and desktop
    SPDocument* _document = nullptr;
    SPDesktop* _desktop = nullptr;

    // Widgets
    Linea::UI::SPDesktopWidget* _desktop_widget = nullptr;

    QToolBar* _toolBar = nullptr;

    // Window state tracking
    Qt::WindowStates _old_window_state = Qt::WindowNoState;

    // Settings
    QSettings* _settings = nullptr;

    // Gio action map backing store
    Glib::RefPtr<Gio::SimpleActionGroup> _action_group{Gio::SimpleActionGroup::create()};
};

#endif // LINEA_MAINWINDOW_H
