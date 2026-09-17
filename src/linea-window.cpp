// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Main window implementation for Linea app.
 */

#include "linea-window.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDebug>
#include <QKeyEvent>
#include <QMenuBar>
#include <QMessageBox>
#include <QResizeEvent>
#include <QShowEvent>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWindow>

#include "actions/action-registry.h"
#include "actions/actions-canvas-mode.h"
#include "actions/actions-canvas-snapping.h"
#include "actions/actions-canvas-transform.h"
#include "actions/actions-dialogs.h"
#include "actions/actions-edit-document.h"
#include "actions/actions-edit-window.h"
#include "actions/actions-edit.h"
#include "actions/actions-effect.h"
#include "actions/actions-file-window.h"
#include "actions/actions-file.h"
#include "actions/actions-hide-lock.h"
#include "actions/actions-layer.h"
#include "actions/actions-node-align.h"
#include "actions/actions-node-options.h"
#include "actions/actions-object-align.h"
#include "actions/actions-object.h"
#include "actions/actions-pages.h"
#include "actions/actions-paths.h"
#include "actions/actions-selection-object.h"
#include "actions/actions-selection-window.h"
#include "actions/actions-selection.h"
#include "actions/actions-text.h"
#include "actions/actions-tools.h"
#include "actions/actions-transform.h"
#include "actions/actions-undo-document.h"
#include "actions/actions-view-mode.h"
#include "actions/actions-view-window.h"
#include "actions/actions-window.h"
#include "desktop.h"
#include "document.h"
#include "linea-application.h"
#include "main-menu.h"
#include "ui/desktop/desktop-widget.h"
#include "ui/shortcut-manager.h"
#include "ui/widget/canvas.h"

LineaWindow::LineaWindow(QSettings* settings, QWidget* parent)
    : QMainWindow(parent)
    , _app{&LineaApplication::instance()}
    , _settings{settings} {
    try {
        _app->set_active_window(this);

        // Load user shortcut customizations before creating actions
        ShortcutManager::instance().load();

        createActions();

        setupUI();
        createToolBar();
        updateTitle();

        // Add document actions
        add_document_actions();

        // restoreSettings() is called in present() after the window is shown,
        // so that restoreGeometry/restoreState work correctly.
    } catch (std::exception& e) {
        qWarning() << "Failed to create main window due to error:" << e.what();
        throw;
    }
}

LineaWindow::~LineaWindow() = default;

void LineaWindow::addDesktop(SPDesktop* desktop) {
    if (!desktop) return;
    // SPDesktopWidget::addDesktop triggers switchDesktop, which calls back into
    // setActiveTab to update _desktop/_document and refresh the title.
    _desktop_widget->addDesktop(desktop);
}

void LineaWindow::setupUI() {
    createMainMenu(menuBar());

    // When no desktop is provided (empty window), pass a null canvas; the first
    // desktop added via addDesktop() will supply its canvas to the widget.
    auto canvas = _desktop ? _desktop->getCanvas() : nullptr;
    _desktop_widget = new Linea::UI::SPDesktopWidget(canvas, this);
    if (_desktop) {
        _desktop_widget->addDesktop(_desktop);
    }
    else {
        // setActiveTab(nullptr);
    }
    setCentralWidget(_desktop_widget);
}

void LineaWindow::createActions() {
    add_actions_tools(this);
    add_actions_file(this);
    add_actions_file_window(_app);
    add_actions_edit_window(_app);
    add_actions_effect(_app);
    add_actions_select_window(_app);
    add_actions_text(_app);
    add_actions_edit(_app);
    add_actions_selection(_app);
    add_actions_selection_object(_app);
    add_actions_hide_lock(_app);
    add_actions_transform(_app);
    add_actions_object_align(_app);
    add_actions_object(_app);
    add_actions_pages(_app);
    add_actions_path(_app);
    add_actions_edit_document(_app);
    add_actions_canvas_snapping(this);
    add_actions_canvas_transform(this);
    add_actions_node_options(this);
    add_actions_canvas_mode(_app);
    add_actions_dialogs(_app);
    add_actions_layer(_app);
    add_actions_node_align(_app);
    add_actions_undo_document(_app);
    add_actions_view_mode(_app);
    add_actions_view_window(_app);
    add_actions_window(_app);
}

void LineaWindow::createToolBar() {}

void LineaWindow::updateTitle() {
    QString title = "Linea";
    if (_desktop && _document) {
        auto docName = QString::fromUtf8(_document->getDocumentName());
        title = QString("%2%1 - Linea").arg(docName).arg(_document->isModifiedSinceSave() ? "*" : "");
    }
    setWindowTitle(title);
}

void LineaWindow::closeEvent(QCloseEvent* event) {
    saveSettings();
    if (_desktop_widget) {
        _desktop_widget->saveSettings();
    }

    if (_document && _document->isModifiedSinceSave()) {
        // QMessageBox::StandardButton reply = QMessageBox::question(
        //     this, "Unsaved Changes", "The document has been modified. Do you want to save changes?",
        //     QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

        // if (reply == QMessageBox::Cancel) {
        //     event->ignore();
        //     return;
        // }
        // TODO: Implement save for Save option
    }

    event->accept();
}

void LineaWindow::keyPressEvent(QKeyEvent* event) {
    // Route key events through the current active desktop's canvas,
    // not the cached _canvas pointer which may be stale after a document swap.
    if (_desktop_widget) {
        if (auto desktop = _desktop_widget->get_desktop()) {
            desktop->getCanvas()->handleKeyPress(event);
        }
    }

    QMainWindow::keyPressEvent(event);
}

// Window state methods
bool LineaWindow::isFullscreen() const {
    return windowState() & Qt::WindowFullScreen;
}

bool LineaWindow::isMaximised() const {
    return windowState() & Qt::WindowMaximized;
}

bool LineaWindow::isMinimized() const {
    return windowState() & Qt::WindowMinimized;
}

void LineaWindow::present() {
    show();
    restoreSettings();
    raise();
    activateWindow();
}

void LineaWindow::toggleColorPalette() {
    if (_desktop_widget) _desktop_widget->toggleColorPalette();
}

bool LineaWindow::colorPaletteVisible() const {
    return _desktop_widget && _desktop_widget->colorPaletteVisible();
}

void LineaWindow::toggleRulers() {
    if (_desktop_widget) _desktop_widget->toggleRulers();
}

bool LineaWindow::rulersVisible() const {
    return _desktop_widget && _desktop_widget->rulersVisible();
}

void LineaWindow::toggleDialogs() {
    if (_desktop_widget) _desktop_widget->toggleDialogs();
}

bool LineaWindow::dialogsVisible() const {
    return _desktop_widget && _desktop_widget->dialogsVisible();
}

bool LineaWindow::getFullscreen() const {
    return isFullscreen();
}

void LineaWindow::toggleFullscreen() {
    if (isFullscreen()) {
        showNormal();
    } else {
        showFullScreen();
    }
}

void LineaWindow::maximize() {
    showMaximized();
}

void LineaWindow::fullscreen() {
    showFullScreen();
}

QScreen* LineaWindow::get_surface() const {
    return windowHandle() ? windowHandle()->screen() : nullptr;
}

int LineaWindow::get_width() const {
    return width();
}

int LineaWindow::get_height() const {
    return height();
}

void LineaWindow::change_document(SPDocument* document) {
    if (!_app) {
        qDebug() << "MainWindow::change_document: app is nullptr!";
        return;
    }

    _document = document;
    _app->set_active_document(_document);
    add_document_actions();

    update_dialogs();
    updateTitle();
}

void LineaWindow::setActiveTab(SPDesktop* desktop) {
    _desktop = desktop;
    _document = _desktop ? _desktop->getDocument() : nullptr;

    _app->set_active_document(_document);
    _app->set_active_desktop(_desktop);
    // _app->set_active_selection(_desktop ? _desktop->getSelection() : nullptr);

    if (_desktop) {
        update_dialogs();
        add_document_actions();
    }

    updateTitle();
}

// Event handlers
void LineaWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);
}

void LineaWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::WindowStateChange) {
        onWindowStateChanged();
    }
    if (event->type() == QEvent::ActivationChange) {
        if (isActiveWindow()) {
            onFocusChanged();
        }
    }
    QMainWindow::changeEvent(event);
}

void LineaWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
}

void LineaWindow::saveSettings() {
    if (!_settings) return;

    _settings->setValue("windowGeometry", saveGeometry());
    _settings->setValue("windowState", saveState());
    _settings->sync();
}

void LineaWindow::restoreSettings() {
    if (!_settings) return;

    restoreGeometry(_settings->value("windowGeometry").toByteArray());
    restoreState(_settings->value("windowState").toByteArray());
}

void LineaWindow::onFocusChanged() {
    if (!isActiveWindow()) {
        return;
    }

    if (!_app) {
        qDebug() << "MainWindow::onFocusChanged: app is nullptr!";
        return;
    }

    // TODO: Need to update InkscapeApplication to accept MainWindow*
    // For now, we update other active state
    _app->set_active_document(_document);
    _app->set_active_desktop(_desktop);
    // _app->set_active_selection(_desktop->getSelection());

    update_dialogs();
}

void LineaWindow::onWindowStateChanged() {
    // Track window state changes
    Qt::WindowStates new_state = windowState();
    _old_window_state = new_state;

    if (_desktop) {
        // Notify desktop of window state change
        // TODO: Add onWindowStateChanged method to SPDesktop if needed
    }
}

void LineaWindow::add_document_actions() {
    // TODO: Implement document action group for Qt
    // This would add actions like save, export, etc. that are document-specific
}

void LineaWindow::update_dialogs() {
    // TODO: Update floating dialogs to point to this window
    // TODO: Update docked dialogs in this window
    // TODO: Add updateDialogs method to SPDesktop or handle here
}

std::vector<Glib::ustring> LineaWindow::list_actions() const {
    std::vector<Glib::ustring> actions;

    if (_action_group) {
        auto action_list = _action_group->list_actions();
        actions.reserve(action_list.size());
        for (const auto& action : action_list) {
            actions.push_back(action);
        }
    }

    return actions;
}
