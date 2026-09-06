// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * This is main window content
 */

#ifndef LINEA_UI_DESKTOP_WIDGET_H
#define LINEA_UI_DESKTOP_WIDGET_H

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QSettings>
#include <QStackedWidget>
#include <QWidget>
#include <string>
#include <unordered_map>
#include <sigc++/scoped_connection.h>
#include <2geom/int-point.h>

#include "message.h"
#include "qt/ui/canvas-frame.h"
#include "qt/ui/color-palette-options.h"
#include "qt/ui/color-palette-panel.h"
#include "qt/ui/color-palette-widget.h"
#include "qt/ui/tab-strip.h"
#include "ui/desktop/left-panel.h"
#include "ui/desktop/right-panel.h"
#include "ui/widget/main-toolbar.h"
#include "ui/widget/notification-bar.h"
#include "ui/widget/overlay-layout.h"

QT_BEGIN_NAMESPACE
class QWindow;
QT_END_NAMESPACE

class QToolButton;
class QLineEdit;

namespace Ui {
class SPDesktopWidget;
}

class LineaWindow;
class SPDesktop;
namespace Inkscape::UI::Widget {
class Canvas;
}

namespace Glib {
class ustring;
}

namespace Linea::UI {
class XmlTreeWidget;
class ObjectTreeView;
class NumberEdit;
class PanelSwitch;
class WelcomePage;
class SPDesktopWidget;
} // namespace Linea::UI

namespace Linea {
struct PresentationState;
}

/**
 * QWidget container wrapper for embedding QWindow (e.g., QOpenGLWindow)
 * in a QWidget hierarchy using QWidget::createWindowContainer.
 */
class Linea::UI::SPDesktopWidget : public QWidget {
    Q_OBJECT

public:
    explicit SPDesktopWidget(Inkscape::UI::Widget::Canvas* canvas, LineaWindow* parent);
    ~SPDesktopWidget() override;

    // Window access
    LineaWindow* get_window() const { return _window; }

    // Canvas access
    Inkscape::UI::Widget::Canvas* get_canvas() const { return _canvas; }

    // Desktop access
    SPDesktop* get_desktop() const { return _desktop; }
    const std::vector<SPDesktop*>& get_desktops() const { return _desktops; }

    // Desktop management
    void addDesktop(SPDesktop* desktop, int pos = -1);
    void removeDesktop(SPDesktop* desktop);
    void switchDesktop(SPDesktop* desktop);
    void advanceTab(int by);
    void refreshTabTitle(SPDesktop* desktop);
    void refreshDocumentTitle(SPDesktop* desktop);
    void connectDocumentTitleSignals(SPDesktop* desktop, SPDocument* doc);

    // Window size
    Geom::IntPoint getWindowSize() const;
    void setWindowSize(const Geom::IntPoint& size);

    // Window presentation
    void presentWindow();
    void setWindowTransient(QWidget& window, int transient_policy = 1);
    void toggleDialogs();
    bool dialogsVisible() const;
    // color palette on the right
    void toggleColorPalette();
    bool colorPaletteVisible() const { return _colorPaletteVisible; }
    // canvas rulers
    void toggleRulers();
    bool rulersVisible() const;
    void updatePanelMargins();

    // Messages and dialogs
    void setMessage(Inkscape::MessageType type, const char* message);
    void showInfoDialog(const Glib::ustring& message);
    bool warnDialog(const Glib::ustring& text);

    // Coordinate status
    void viewSetPosition(Geom::Point p);
    void setCoordinateStatus(Geom::Point p);

    // Canvas updates
    void updateZoom(double zoom);
    void updateRotation(double angle);
    void updateRulers();
    void toggleScrollbars();

    // Focus
    void onFocus(bool has_focus);

    // toolbars
    Glib::ustring* get_toolbar_by_name(const Glib::ustring& name) { return 0; }

    // Settings
    void saveSettings();
    void restoreSettings();

    // Notifications
    void showInfo(const QString& operation, const QString& msg);
    void showError(const QString& operation, const QString& msg);
    void showError(const QString& operation, const QString& filename, const QString& msg);
protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    bool dialogsDocked() const;
    void dockPanels(bool dock);
    void showTabStrip();
    void updateEmptyStateVisibility();

    Ui::SPDesktopWidget* _ui = nullptr;
    LineaWindow* _window = nullptr;
    Inkscape::UI::Widget::Canvas* _canvas = nullptr; // canvas of the first desktop (bootstrap only)
    CanvasFrame* _canvasFrame = nullptr;             // hosts rulers + canvas stack
    std::unordered_map<SPDesktop*, Inkscape::UI::Widget::Canvas*> _canvasForDesktop; // desktop → its canvas
    LeftPanel* _leftPanel = nullptr;
    RightPanel* _rightPanel = nullptr;
    MainToolbar* _toolbar = nullptr;
    WelcomePage* _welcomePage = nullptr;
    ColorPaletteWidget* _colorPalette = nullptr;
    ColorPalettePanel* _colorPalettePanel = nullptr;
    NotificationBar* _notificationBar = nullptr;

    // Desktop management
    std::vector<SPDesktop*> _desktops;
    SPDesktop* _desktop = nullptr;
    TabStrip* _tabStrip = nullptr;
    std::unordered_map<SPDesktop*, QWidget*> _tabHandles;

    // Per-desktop signal connections
    std::unordered_map<SPDesktop*, sigc::scoped_connection> _docModifiedConns;
    std::unordered_map<SPDesktop*, sigc::scoped_connection> _docFilenameConns;
    std::unordered_map<SPDesktop*, sigc::scoped_connection> _docReplacedConns;
    std::unordered_map<SPDesktop*, sigc::scoped_connection> _desktopDestroyConns;

    QWidget* _tabForDesktop(SPDesktop* desktop) const;
    QString _tabTitle(SPDesktop* desktop) const;

    // Update all sidebars to reflect the given desktop (or null to clear them)
    void _updatePanelsForDesktop(SPDesktop* desktop);
    void _clearPanels();
    void _updateBothPanels();

    // Color palette
    void _applyPalette(int index);
    void _applyTileSize(Linea::UI::ColorPaletteOptions::TileSize size);

    // Pages tool auto-switching
    void _switchToPagesTool();
    void _restorePreviousTool();

    // Settings
    QSettings* _settings = nullptr;
    QWidget* _canvasContainer = nullptr;
    OverlayLayout* _overlayLayout = nullptr;

    // Selection sync
    sigc::scoped_connection _selectionChanged;
    sigc::scoped_connection _styleChanged;
    sigc::scoped_connection _desktopStyleChanged;
    sigc::scoped_connection _desktopToolChanged;

    bool _colorPaletteVisible = true;

    std::string _pagesPreviousTool;
};

#endif // LINEA_UI_DESKTOP_WIDGET_H
