// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * QWidget container wrapper implementation.
 */

#include "desktop-widget.h"

#include <QActionGroup>
#include <QDataStream>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMessageBox>
#include <QMimeData>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <giomm.h>
#include <vector>

#include "actions/action-registry.h"
#include "actions/actions-tools.h"
#include "color-palette-options.h"
#include "color-palette-panel.h"
#include "color-palette-widget.h"
#include "colors/color.h"
#include "desktop-style.h"
#include "ui/clipboard.h"
#include "desktop.h"
#include "dialog/xml-settings-dialog.h"
#include "document-undo.h"
#include "document.h"
#include "layer-manager.h"
#include "linea-application.h"
#include "object-tree-view.h"
#include "object/sp-namedview.h"
#include "preferences.h"
#include "qt/ui/document-properties-panel.h"
#include "qt/ui/panel-switch.h"
#include "qt/ui/welcome-page.h"
#include "qt/ui/tab-strip.h"
#include "selection.h"
#include "style-internal.h"
#include "ui/dialog/global-palettes.h"
#include "ui/desktop/update-paint-indicator.h"
#include "ui/drag-and-drop.h"
#include "ui/util.h"
#include "ui/widget/canvas.h"
#include "ui/widget/collapsible-panel.h"
#include "ui/widget/main-toolbar.h"
#include "ui/widget/overlay-layout.h"
#include "ui_desktop-widget.h"
#include "util/style-utils.h"

constexpr int PANEL_MARGIN = 10;

namespace Linea::UI {

/// Map a ColorPaletteOptions::TileSize to pixel size for the palette swatches.
static int tileSizePixels(ColorPaletteOptions::TileSize size) {
    switch (size) {
        case ColorPaletteOptions::TileSize::Small:
            return 11;
        case ColorPaletteOptions::TileSize::Medium:
            return 15;
        case ColorPaletteOptions::TileSize::Large:
            return 21;
    }
    return 15;
}

SPDesktopWidget::SPDesktopWidget(Inkscape::UI::Widget::Canvas* canvas, LineaWindow* parent)
    : QWidget(parent)
    , _ui(new Ui::SPDesktopWidget())
    , _window(parent)
    , _canvas(canvas)
    , _settings{new QSettings(QSettings::UserScope, "Linea", "LineaDraw", this)} {
    _ui->setupUi(this);

    // canvasContainer and CanvasFrame come from the UI file. CanvasFrame hosts
    // the tab strip in its header row (created in its constructor).
    _canvasContainer = _ui->canvasContainer;
    _canvasFrame = _ui->canvasFrame;

    _tabStrip = _canvasFrame->tabStrip();
    _tabStrip->setVisible(false); // hidden until a second desktop is added

    // Tab-strip signal connections are set up here once; they reference
    // _desktops/_tabHandles which are populated later by addDesktop().
    connect(_tabStrip, &TabStrip::tabSelectRequested, this, [this](QWidget* tab) {
        for (auto& [dt, handle] : _tabHandles) {
            if (handle == tab) {
                switchDesktop(dt);
                return;
            }
        }
    });

    connect(_tabStrip, &TabStrip::tabCloseRequested, this, [this](QWidget* tab) {
        for (auto& [dt, handle] : _tabHandles) {
            if (handle == tab) {
                LINEA_APP.destroyDesktop(dt, true);
                return;
            }
        }
    });

    connect(_tabStrip, &TabStrip::tabFloatRequested, this, [this](QWidget* tab) {
        for (auto& [dt, handle] : _tabHandles) {
            if (handle == tab) {
                LINEA_APP.detachDesktopToNewWindow(dt);
                return;
            }
        }
    });

    connect(_tabStrip, &TabStrip::tabRearranged, this, [this](int from, int to) {
        if (from == to || from < 0 || to < 0) return;
        if (from >= static_cast<int>(_desktops.size())) return;
        // Reorder _desktops to mirror the new strip order
        auto moved = _desktops[from];
        _desktops.erase(_desktops.begin() + from);
        int dst = to > from ? to - 1 : to;
        dst = std::clamp(dst, 0, static_cast<int>(_desktops.size()));
        _desktops.insert(_desktops.begin() + dst, moved);
    });

    connect(_tabStrip, &TabStrip::tabMoveRequested, this,
            [this](QWidget* tab, int /*srcIdx*/, TabStrip* srcStrip, int dstIdx) {
                // Find the source SPDesktopWidget via the strip's parent chain
                auto srcDW = qobject_cast<SPDesktopWidget*>(srcStrip->parent());
                if (!srcDW) return;

                SPDesktop* dt = nullptr;
                for (auto& [d, h] : srcDW->_tabHandles) {
                    if (h == tab) {
                        dt = d;
                        break;
                    }
                }
                if (!dt) return;

                srcDW->removeDesktop(dt);
                addDesktop(dt, dstIdx);
            });

    // Create overlay layout for canvas and panels, parented to the container.
    // OverlayLayout positions CanvasFrame as the first (canvas) widget that
    // panels overlap / dock against.
    _overlayLayout = new OverlayLayout(_canvasContainer);
    _overlayLayout->addWidget(_canvasFrame);

    _welcomePage = new WelcomePage(_canvasContainer);
    _welcomePage->setMinimumWidth(400);
    _welcomePage->setMaximumWidth(800);
    _welcomePage->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    _welcomePage->layout()->setContentsMargins(10, 55, 10, 50);
    _overlayLayout->setPanelPosition(_welcomePage, OverlayLayout::Position::Center, QSize(640, 480), true,
                                     {PANEL_MARGIN, PANEL_MARGIN, PANEL_MARGIN, PANEL_MARGIN});

    _colorPalettePanel = new ColorPalettePanel(_canvasContainer);
    _colorPalette = _colorPalettePanel->palette();
    _colorPalette->setAlignment(ColorPaletteWidget::Alignment::Vertical);
    _colorPalette->setMargins(6, 5, 0, 5);
    _colorPalettePanel->setResizableEdge(Qt::Edge::LeftEdge);
    connect(_colorPalette, &ColorPaletteWidget::cellClicked, this,
            [this](const auto& color, Qt::KeyboardModifiers modifiers, Qt::MouseButtons buttons) {
                bool stroke = !!(buttons & Qt::LeftButton) == !!(modifiers & Qt::ShiftModifier);
                if (_desktop) {
                    sp_desktop_apply_color(_desktop, color, stroke, _desktop->getTool());
                }
            });
    _overlayLayout->addWidget(_colorPalettePanel);
    auto optWidth = _colorPalette->widthForColumns(1);

    _overlayLayout->setPanelPosition(_colorPalettePanel, OverlayLayout::Position::Right, QSize(optWidth, 0), true);

    // Basic colors (None / black / gray / white) shown above the palette swatches.
    using ColorItem = Inkscape::UI::Dialog::PaletteFileData::ColorItem;
    std::vector<ColorItem> basicColors{
        ColorItem{Inkscape::UI::Dialog::PaletteFileData::None{}},
        ColorItem{Inkscape::Colors::Color(0x000000ff, false)},
        ColorItem{Inkscape::Colors::Color(0x7f7f7fff, false)},
        ColorItem{Inkscape::Colors::Color(0xffffffff, false)},
    };
    _colorPalette->setBasicColors(std::move(basicColors));

    // Wire the options popup so palette / tile-size changes apply live.
    auto options = _colorPalettePanel->options();
    connect(options, &Linea::UI::ColorPaletteOptions::paletteChanged, this,
            [this](int index) { _applyPalette(index); });
    connect(options, &Linea::UI::ColorPaletteOptions::tileSizeChanged, this,
            [this](Linea::UI::ColorPaletteOptions::TileSize size) { _applyTileSize(size); });

    // Create left and right panels
    _leftPanel = new LeftPanel(_canvasContainer);
    _overlayLayout->setPanelPosition(_leftPanel, OverlayLayout::Position::Left, QSize(300, 400), true, {PANEL_MARGIN, 0, PANEL_MARGIN, PANEL_MARGIN});

    _rightPanel = new RightPanel(_canvasContainer);
    _overlayLayout->setPanelPosition(_rightPanel, OverlayLayout::Position::Right, QSize(300, 400), true,
                                     {0, PANEL_MARGIN, PANEL_MARGIN, PANEL_MARGIN});

    // Connect resize signals to update layout
    connect(_leftPanel, &CollapsiblePanel::resized, this,
            [this](int) { _overlayLayout->updatePanelGeometry(); });
    connect(_rightPanel, &CollapsiblePanel::resized, this,
            [this](int) { _overlayLayout->updatePanelGeometry(); });
    connect(_colorPalettePanel, &ColorPalettePanel::resized, this,
            [this](int) { _overlayLayout->updatePanelGeometry(); });

    // Create toolbar and place in center overlay
    _toolbar = new MainToolbar(_canvasContainer);
    Inkscape::UI::add_drop_shadow(_toolbar);
    _overlayLayout->setPanelPosition(_toolbar, OverlayLayout::Position::Center, QSize(0, 0), false, {0, 0, PANEL_MARGIN, 0});

    _notificationBar = new NotificationBar(_canvasContainer);
    _notificationBar->setFixedSize(400, 40);
    _overlayLayout->setPanelPosition(_notificationBar, OverlayLayout::Position::Center, QSize(400, 40), false, {0, 0, PANEL_MARGIN + 70, 0});
    _overlayLayout->setPanelMode(_notificationBar, OverlayLayout::Mode::Floating);
    _notificationBar->hide();

    // _notificationBar->setText("<b>Document saved</b>", "test notification");
    // _notificationBar->setIcon(QIcon(":/icons/warning-white"));
    // _notificationBar->setSeverity("error");
    // _notificationBar->showBar();

    // _chromeButton = new QToolButton(_canvasContainer);
    // _chromeButton->setProperty("class", "attention-button");
    // _chromeButton->setIcon(QIcon(":/icons/edit-undo"));
    // _chromeButton->setToolTip(tr("Restore dialogs"));
    // _chromeButton->setVisible(false);
    // _chromeButton->setFixedSize(24, 24);
    // _overlayLayout->setPanelPosition(_chromeButton, OverlayLayout::Position::Right, QSize(24, 24), false, {3, 3, 3, 3});
    // _overlayLayout->setPanelMode(_chromeButton, OverlayLayout::Mode::Floating);
    // connect(_chromeButton, &QToolButton::clicked, this, [this](bool) { toggleDialogs(); });

    // Adjust panel top margins to account for ruler height
    updatePanelMargins();

    // --- Actions ---

    auto& registry = ActionRegistry::get();

    //TODO: remove actions from here

    auto toggle_xml_tree = registry.createToggleAction(
        {"toggle-svg-tree", "Switch ", "Switch between XML tree and object tree", "document-metadata"},
        [this](bool on) {
            _leftPanel->switchWidget()->setCurrentIndex(on ? 1 : 0);
        },
        [this]() -> bool {
            return _leftPanel->switchWidget()->currentIndex() == 1;
        }, "dialog-objects");
    window()->addAction(toggle_xml_tree);

    window()->addAction(
        registry.createAction(ActionMeta{"open-settings", "Open Settings", "Open the settings dialog", "settings"}, [this] {
            auto dialog = std::make_unique<Linea::UI::XmlSettingsDialog>(this);
            dialog->load(QDir::homePath() + "/.config/linea/preferences.xml");
            dialog->exec();
        }));

    // Build panel headers now that actions are registered.
    _leftPanel->buildHeader();
    _rightPanel->buildHeader();

    // --- Wire panel signals ---

    // Zoom from right panel
    connect(_rightPanel, &RightPanel::zoomChanged, this, [this](double zoom) {
        if (_desktop) _desktop->zoom_absolute(_desktop->current_center(), zoom, true);
    });
    // Rotation from right panel
    connect(_rightPanel, &RightPanel::rotationChanged, this, [this](double angle) {
        if (_desktop) _desktop->rotate(angle);
    });

    // Object tree → pages tool / restore tool
    connect(_leftPanel, &LeftPanel::objectsSelected, this, [this](std::vector<SPObject*> objects) {
        if (!objects.empty()) _restorePreviousTool();
    });
    connect(_leftPanel, &LeftPanel::virtualNodeSelected, this, [this](int typeInt) {
        auto type = static_cast<VirtualNodeType>(typeInt);
        if (type == VirtualNodeType::Pages) {
            _switchToPagesTool();
        } else {
            _restorePreviousTool();
        }
        _rightPanel->propertiesPanel()->showProperties(type);
    });

    // Sync toggle-svg-tree action when panel switch changes
    connect(_leftPanel, &LeftPanel::panelSwitchChanged, this, [this](int index) {
        if (auto action = ActionRegistry::get().action("toggle-svg-tree")) {
            action->setChecked(index == 1);
        }
    });

    setAcceptDrops(true);

    restoreSettings();

    // Hide the toolbar when starting empty (no desktop); it will be shown
    // again by addDesktop() once a document is loaded.
    updateEmptyStateVisibility();
}

SPDesktopWidget::~SPDesktopWidget() {
    delete _ui;
}

Geom::IntPoint SPDesktopWidget::getWindowSize() const {
    return Geom::IntPoint(QWidget::width(), QWidget::height());
}

void SPDesktopWidget::setWindowSize(const Geom::IntPoint& size) {
    QWidget::resize(size.x(), size.y());
}

// Desktop management helpers

QWidget* SPDesktopWidget::_tabForDesktop(SPDesktop* desktop) const {
    auto it = _tabHandles.find(desktop);
    return it != _tabHandles.end() ? it->second : nullptr;
}

QString SPDesktopWidget::_tabTitle(SPDesktop* desktop) const {
    if (!desktop) return {};
    auto doc = desktop->getDocument();
    if (!doc) return {};

    QString title = QString::fromUtf8(doc->getDocumentName());
    if (doc->isModifiedSinceSave()) title.prepend('*');
    return title;
}

void SPDesktopWidget::refreshDocumentTitle(SPDesktop* desktop) {
    if (!desktop) return;

    refreshTabTitle(desktop);

    if (desktop == _desktop && _window) {
        _window->updateTitle();
    }
}

void SPDesktopWidget::connectDocumentTitleSignals(SPDesktop* desktop, SPDocument* doc) {
    if (!desktop || !doc) return;

    _docModifiedConns[desktop] =
        doc->connectSavedOrModified([this, desktop] { refreshDocumentTitle(desktop); });
    _docFilenameConns[desktop] =
        doc->connectFilenameSet([this, desktop](char const*) { refreshDocumentTitle(desktop); });
}

void SPDesktopWidget::refreshTabTitle(SPDesktop* desktop) {
    if (!desktop) return;
    auto tab = _tabForDesktop(desktop);
    if (!tab) return;

    const QString title = _tabTitle(desktop);
    _tabStrip->setTabLabel(*tab, title);
    _tabStrip->setTabTooltip(*tab, title);
}

void SPDesktopWidget::showTabStrip() {
    auto visible = _desktops.size() > 1;
    if (_tabStrip->isVisible() != visible) {
        _tabStrip->setVisible(visible);
        // Re-apply dock state so toolbar mode is updated (floating when tabstrip visible)
        dockPanels(dialogsDocked());
    }
}

void SPDesktopWidget::addDesktop(SPDesktop* desktop, int pos) {
    if (!desktop) return;

    if (pos < 0 || pos > static_cast<int>(_desktops.size())) {
        pos = static_cast<int>(_desktops.size());
        _desktops.push_back(desktop);
    } else {
        _desktops.insert(_desktops.begin() + pos, desktop);
    }

    desktop->setDesktopWidget(this);

    // Add a tab for this desktop and remember the handle
    const QString title = _tabTitle(desktop);
    auto tab = _tabStrip->addTab(title, {}, pos);
    _tabStrip->setTabTooltip(*tab, title);
    _tabHandles[desktop] = tab;

    // Register the desktop's canvas as a page in the canvas stack
    auto canvasWidget = desktop->getCanvas();

    // If this is the first desktop and its canvas is the same as the constructor's canvas,
    // make sure the desktop adapter is set properly for gesture handling
    if (!_canvas) {
        _canvas = canvasWidget;
    }
    if (_canvas == canvasWidget) {
        _canvas->setDesktop(desktop);
    }

    _canvasFrame->addCanvas(canvasWidget);
    _canvasForDesktop[desktop] = canvasWidget;

    // Subscribe to the document's save-state and filename signals so both the
    // tab label and the window title stay in sync.
    connectDocumentTitleSignals(desktop, desktop->getDocument());

    showTabStrip();
    updateEmptyStateVisibility();
    switchDesktop(desktop);
}

void SPDesktopWidget::updateEmptyStateVisibility() {
    assert(_toolbar);
    assert(_welcomePage);
    _toolbar->setVisible(_desktop != nullptr);
    _canvasFrame->setVisible(_desktop != nullptr);
    _welcomePage->setVisible(_desktops.empty());
}

void SPDesktopWidget::removeDesktop(SPDesktop* desktop) {
    if (!desktop) return;

    auto it = std::find(_desktops.begin(), _desktops.end(), desktop);
    if (it == _desktops.end()) return;

    // Determine a replacement active desktop before erasing
    SPDesktop* next = nullptr;
    if (_desktop == desktop && _desktops.size() > 1) {
        int idx = static_cast<int>(std::distance(_desktops.begin(), it));
        int nextIdx = (idx + 1 < static_cast<int>(_desktops.size())) ? idx + 1 : idx - 1;
        next = _desktops[nextIdx];
    }

    // Clear panels before the desktop/document is freed if it was the active one
    if (_desktop == desktop) _clearPanels();

    _desktops.erase(it);
    desktop->setDesktopWidget(nullptr);

    // Remove tab and clean up all per-desktop connections / canvas page
    if (auto tab = _tabForDesktop(desktop)) {
        _tabStrip->removeTab(*tab);
    }
    if (auto it = _canvasForDesktop.find(desktop); it != _canvasForDesktop.end()) {
        _canvasFrame->removeCanvas(it->second);
        _canvasForDesktop.erase(it);
    }
    _tabHandles.erase(desktop);
    _docModifiedConns.erase(desktop);
    _docFilenameConns.erase(desktop);
    _docReplacedConns.erase(desktop);
    _desktopDestroyConns.erase(desktop);

    showTabStrip();

    if (_desktop == desktop) {
        _desktop = nullptr;
        updateEmptyStateVisibility();
        if (next) {
            switchDesktop(next);
        } else if (_window) {
            // Clear the window's cached document/desktop before the desktop is
            // destroyed; the window itself remains open and empty.
            _window->setActiveTab(nullptr);
        }
    }

    if (_canvas == desktop->getCanvas()) {
        _canvas = nullptr;
    }
}

void SPDesktopWidget::_clearPanels() {
    _selectionChanged.disconnect();
    _styleChanged.disconnect();
    _desktopStyleChanged.disconnect();
    _desktopToolChanged.disconnect();

    _leftPanel->clear();
    _rightPanel->clear();
}

void SPDesktopWidget::_updatePanelsForDesktop(SPDesktop* desktop) {
    if (!desktop) {
        _clearPanels();
        _canvasFrame->setCurrentCanvas(nullptr, nullptr);
        return;
    }

    // Swap the visible canvas page to the one belonging to this desktop
    if (auto it = _canvasForDesktop.find(desktop); it != _canvasForDesktop.end()) {
        _canvasFrame->setCurrentCanvas(desktop, it->second);
    }
    else {
        // warning
        g_warning("SPDesktopWidget: Desktop not found in canvas map!");
        _clearPanels();
        _canvasFrame->setCurrentCanvas(nullptr, nullptr);
        return;
    }

    // Delegate panel content updates to the panel classes
    _leftPanel->setDesktop(desktop);
    _rightPanel->setDesktop(desktop);

    _selectionChanged = desktop->getSelection()->connectChanged([this](Inkscape::Selection* sel) {
        _leftPanel->updateSelectionFromDesktop();
        _updateBothPanels();
        if (sel && !sel->isEmpty()) {
            _restorePreviousTool();
        }
    });

    _styleChanged = desktop->connectSelectionStyleChanged([this](auto& args) {
        refreshPaintIndicator(_toolbar->paintIndicator(), _colorPalette, _desktop, args.presentation);
    });

    _desktopStyleChanged = desktop->connectDesktopStyleChanged([this](auto css) {
        updatePaintIndicator(_toolbar->paintIndicator(), _desktop);
    });

    _desktopToolChanged = desktop->connectEventContextChanged([this](auto desk, auto tool) {
        // selection takes precedent over tool style: don't update if there's a selection
        if (_desktop && !_desktop->getSelection()->isEmpty()) return;

        updatePaintIndicator(_toolbar->paintIndicator(), _desktop);
    });

    _updateBothPanels();
    refreshPaintIndicator(_toolbar->paintIndicator(), _colorPalette, desktop,
                          Linea::query_style_properties(desktop->getSelection()->items()));
}

void SPDesktopWidget::_updateBothPanels() {
    bool docked = _overlayLayout->panelMode(_rightPanel) == OverlayLayout::Mode::Docked;
    bool has_selection = _desktop && _desktop->getSelection() && !_desktop->getSelection()->isEmpty();
    bool virtual_selected = _leftPanel->objectTreeView()->selectedVirtualNode() != VirtualNodeType::None;
    bool collapse = !docked && !has_selection && !virtual_selected;

    _rightPanel->setCollapsed(collapse);
    _leftPanel->setCollapsed(!docked);
}

void SPDesktopWidget::_switchToPagesTool() {
    if (!_desktop || _desktop->getActiveTool() == "Pages") return;

    _pagesPreviousTool = _desktop->getActiveTool();
    _desktop->setTool("Pages");
}

void SPDesktopWidget::_restorePreviousTool() {
    if (!_desktop || _pagesPreviousTool.empty()) return;

    if (_desktop->getActiveTool() != "Pages") {
        _pagesPreviousTool.clear();
        return;
    }
    _desktop->setTool(_pagesPreviousTool);
    _pagesPreviousTool.clear();
}

void SPDesktopWidget::switchDesktop(SPDesktop* desktop) {
    if (!desktop || desktop == _desktop) return;
    if (std::find(_desktops.begin(), _desktops.end(), desktop) == _desktops.end()) return;

    _restorePreviousTool();

    _desktop = desktop;
    updateEmptyStateVisibility();

    // Sync tab strip selection
    if (auto tab = _tabForDesktop(desktop)) {
        _tabStrip->selectTab(*tab);
    }

    if (_window) _window->setActiveTab(desktop);

    _updatePanelsForDesktop(desktop);

    // Check the tool action for the new desktop (QActionGroup unchecks the
    // rest). All other stateful actions are re-evaluated via syncAllActions
    // since the entire desktop context changed.
    auto toolName = desktop->getActiveTool();
    if (auto actionId = tool_action_id(toolName); !actionId.empty()) {
        if (auto action = ActionRegistry::get().action(actionId)) {
            action->setChecked(true);
        }
    }
    ActionRegistry::get().syncAllActions();

    // Ensure paint indicator reflects the new desktop's current tool style
    updatePaintIndicator(_toolbar->paintIndicator(), _desktop);

    // Subscribe to document replacement on this desktop so all panels receive
    // setDocument(newDoc) automatically when the same desktop changes its document.
    _docReplacedConns[desktop] = desktop->connectDocumentReplaced([this, desktop](SPDesktop*, SPDocument* newDoc) {
        if (desktop == _desktop) _updatePanelsForDesktop(desktop);
        // Re-connect title-relevant signals to the new document and refresh
        connectDocumentTitleSignals(desktop, newDoc);
        refreshDocumentTitle(desktop);
    });

    // If the desktop is destroyed while it is the active one, clear the UI
    // before any dangling pointer can be accessed.
    _desktopDestroyConns[desktop] = desktop->connectDestroy([this, desktop](SPDesktop*) {
        if (desktop == _desktop) _clearPanels();
    });

    //TODO: refresh all panels, they are stale

}

void SPDesktopWidget::advanceTab(int by) {
    if (_desktops.empty()) return;

    auto it = std::find(_desktops.begin(), _desktops.end(), _desktop);
    if (it == _desktops.end()) return;

    int current = static_cast<int>(std::distance(_desktops.begin(), it));
    int next = (current + by) % static_cast<int>(_desktops.size());
    if (next < 0) next += static_cast<int>(_desktops.size());

    switchDesktop(_desktops[next]);
}

// Window presentation
void SPDesktopWidget::presentWindow() {
    if (_window) {
        _window->show();
        _window->raise();
        _window->activateWindow();
    }
}

void SPDesktopWidget::setWindowTransient(QWidget& window, int transient_policy) {
    // TODO: Implement transient window handling for Qt
    (void)window;
    (void)transient_policy;
}

// Messages and dialogs
void SPDesktopWidget::setMessage(Inkscape::MessageType type, const char* message) {
    // TODO: Implement message display in status bar
    (void)type;
    (void)message;
}

void SPDesktopWidget::showInfoDialog(const Glib::ustring& message) {
    QMessageBox::information(this, "Inkscape", QString::fromUtf8(message.c_str()));
}

bool SPDesktopWidget::warnDialog(const Glib::ustring& text) {
    auto result =
        QMessageBox::warning(this, "Inkscape", QString::fromUtf8(text.c_str()), QMessageBox::Ok | QMessageBox::Cancel);
    return result == QMessageBox::Ok;
}

// Notifications
void SPDesktopWidget::showInfo(const QString& operation, const QString& msg) {
    _notificationBar->setSeverity("info");
    _notificationBar->setText(operation, msg);
    _notificationBar->setIcon(QIcon(":/icons/warning-white"));
    _notificationBar->showBar();
}

void SPDesktopWidget::showError(const QString& operation, const QString& filename, const QString& msg) {
    _notificationBar->setSeverity("error");
    auto title = filename.isEmpty()
        ? tr("<b>%1</b>").arg(operation.toHtmlEscaped())
        : tr("<b>%1 “%2”</b>")
              .arg(operation.toHtmlEscaped())
              .arg(filename.toHtmlEscaped());
    _notificationBar->setText(title, msg);
    _notificationBar->setIcon(QIcon(":/icons/warning-white"));
    _notificationBar->showBar();
}

void SPDesktopWidget::showError(const QString& operation, const QString& msg) {
    showError(operation, {}, msg);
}

// Coordinate status
void SPDesktopWidget::viewSetPosition(Geom::Point p) {
    // TODO: Implement position display update
    (void)p;
}

void SPDesktopWidget::setCoordinateStatus(Geom::Point p) {
    // TODO: Implement coordinate status update
    (void)p;
}

void SPDesktopWidget::updateZoom(double zoom) {
    // update zoom status in the UI
    _rightPanel->updateZoom(zoom);
}

void SPDesktopWidget::updateRotation(double angle) {
    // update rotation status in the UI
    _rightPanel->updateRotation(angle);
}

void SPDesktopWidget::toggleScrollbars() {
    // TODO: Implement scrollbar visibility toggle
}

void SPDesktopWidget::updatePanelMargins() {
    const bool rulers = _canvasFrame->rulersVisible();
    const int extra = rulers ? _canvasFrame->rulerSize() : 0;
    const int tabStrip = _tabStrip->isVisible() ? _tabStrip->height() : 0;
    constexpr int margin = PANEL_MARGIN;
    auto top_margin = dialogsDocked() ? extra + tabStrip : margin + extra + tabStrip;
    // Base margins: {left, right, top, bottom} — add ruler height + tab strip to top
    _overlayLayout->setPanelMargins(_leftPanel, {margin + extra, 0, margin + extra + tabStrip, margin});
    _overlayLayout->setPanelMargins(_rightPanel, {0, margin, margin + extra + tabStrip, margin});
    _overlayLayout->setPanelMargins(_toolbar, {0, 0, top_margin, 0}, false);
    _overlayLayout->updatePanelGeometry();
}

void SPDesktopWidget::toggleRulers() {
    _canvasFrame->setRulersVisible(!_canvasFrame->rulersVisible());
    // Re-apply dock state so toolbar mode is updated (floating when rulers visible)
    dockPanels(dialogsDocked());
}

bool SPDesktopWidget::rulersVisible() const {
    return _canvasFrame->rulersVisible();
}

void SPDesktopWidget::updateRulers() {
    _canvasFrame->updateRulers();
}

bool SPDesktopWidget::dialogsVisible() const {
    return dialogsDocked();
}

void SPDesktopWidget::toggleDialogs() {
    dockPanels(!dialogsDocked());
}

void SPDesktopWidget::dockPanels(bool dock) {
    auto mode = dock ? OverlayLayout::Mode::Docked : OverlayLayout::Mode::Floating;
    _overlayLayout->setPanelMode(_leftPanel, mode);
    _overlayLayout->setPanelMode(_rightPanel, mode);
    // Toolbar is always floating when rulers or tabstrip are visible
    bool visible = _canvasFrame->rulersVisible() || _canvasFrame->tabStrip()->isVisible();
    _overlayLayout->setPanelMode(_toolbar, visible ? OverlayLayout::Mode::Floating : mode);
    _overlayLayout->activate();
    _leftPanel->setRounded(!dock);
    _rightPanel->setRounded(!dock);
    _toolbar->setRounded(!dock);
    _toolbar->setStretch(dock);
    _colorPalettePanel->setVisible(dock && _colorPaletteVisible);
    updatePanelMargins();
    _updateBothPanels();
}

// Focus
void SPDesktopWidget::onFocus(bool has_focus) {
    // TODO: Implement focus handling
    (void)has_focus;
}

bool SPDesktopWidget::dialogsDocked() const {
    return _overlayLayout->panelMode(_leftPanel) == OverlayLayout::Mode::Docked;
}

void SPDesktopWidget::_applyPalette(int index) {
    const auto& palettes = Inkscape::UI::Dialog::GlobalPalettes::get().palettes();
    if (index < 0 || index >= static_cast<int>(palettes.size())) return;

    _colorPalette->setPaletteData(palettes[index]);
}

void SPDesktopWidget::_applyTileSize(Linea::UI::ColorPaletteOptions::TileSize size) {
    // Preserve the current number of columns across the tile size change
    // by resizing the panel to fit the same column count at the new pitch.
    int columns = _colorPalette->columnsForWidth(_colorPalettePanel->width());
    _colorPalette->setTileSize(tileSizePixels(size));
    _colorPalettePanel->updateButtonMargin();
    _colorPalettePanel->setMinimumWidth(_colorPalette->widthForColumns(1));
    _colorPalettePanel->setMaximumWidth(_colorPalette->widthForColumns(8));
    int newWidth = _colorPalette->widthForColumns(std::max(1, columns));
    _colorPalettePanel->resize(newWidth, _colorPalettePanel->height());
    _overlayLayout->updatePanelGeometry();
}

void SPDesktopWidget::saveSettings() {
    if (!_settings) return;

    _settings->beginGroup("DesktopWidget");
    _leftPanel->saveSettings(*_settings);
    _rightPanel->saveSettings(*_settings);
    _settings->setValue("colorPaletteWidth", _colorPalettePanel->width());
    _settings->setValue("colorPaletteVisible", _colorPaletteVisible);
    _settings->setValue("showDialogIndex", _rightPanel->switchWidget()->currentIndex());
    _settings->setValue("dockedDialogs", dialogsDocked());
    _settings->setValue("rulersVisible", _canvasFrame->rulersVisible());

    // Color palette options
    auto options = _colorPalettePanel->options();
    const auto& palettes = Inkscape::UI::Dialog::GlobalPalettes::get().palettes();
    int palIndex = options->selectedPaletteIndex();
    if (palIndex >= 0 && palIndex < static_cast<int>(palettes.size())) {
        _settings->setValue("colorPaletteId", QString::fromStdString(palettes[palIndex].id.raw()));
    }
    _settings->setValue("colorPaletteTileSize", static_cast<int>(options->tileSize()));

    auto toggleSvgTreeAction = ActionRegistry::get().action("toggle-svg-tree");
    _settings->setValue("toggleSvgTree", toggleSvgTreeAction->isChecked());

    _settings->endGroup();
    _settings->sync();
}

void SPDesktopWidget::restoreSettings() {
    _settings->beginGroup("DesktopWidget");
    _leftPanel->restoreSettings(*_settings);
    _rightPanel->restoreSettings(*_settings);
    int colorPaletteWidth = _settings->value("colorPaletteWidth", _colorPalette->widthForColumns(1)).toInt();
    _colorPaletteVisible = _settings->value("colorPaletteVisible", true).toBool();
    bool toggleSvgTree = _settings->value("toggleSvgTree", false).toBool();
    int showDialog = _settings->value("showDialogIndex", 0).toInt();
    bool dockedDialogs = _settings->value("dockedDialogs", false).toBool();
    bool rulersVisible = _settings->value("rulersVisible", true).toBool();
    QString paletteId = _settings->value("colorPaletteId").toString();
    int tileSizeIndex = _settings->value("colorPaletteTileSize", static_cast<int>(ColorPaletteOptions::TileSize::Medium)).toInt();
    _settings->endGroup();

    _colorPalettePanel->resize(colorPaletteWidth, _colorPalettePanel->height());

    _overlayLayout->updatePanelGeometry();

    _canvasFrame->setRulersVisible(rulersVisible);
    dockPanels(dockedDialogs);

    _leftPanel->switchWidget()->setCurrentIndex(toggleSvgTree ? 1 : 0);
    _rightPanel->switchWidget()->setCurrentIndex(showDialog);

    //TODO: fix this action
    auto toggleSvgTreeAction = ActionRegistry::get().action("toggle-svg-tree");
    toggleSvgTreeAction->setChecked(toggleSvgTree);

    // Restore color palette selection and tile size. We set the options widget
    // state for UI consistency and apply directly to the palette widget, since
    // signals may not fire when the value is unchanged from the default.
    auto options = _colorPalettePanel->options();
    const auto& palettes = Inkscape::UI::Dialog::GlobalPalettes::get().palettes();
    int palIndex = 0; // default to first palette
    if (!paletteId.isEmpty()) {
        const auto id = paletteId.toStdString();
        for (int i = 0; i < static_cast<int>(palettes.size()); ++i) {
            if (palettes[i].id.raw() == id) {
                palIndex = i;
                break;
            }
        }
    }
    if (!palettes.empty()) {
        options->setSelectedPaletteIndex(palIndex);
        _applyPalette(palIndex);
    }
    auto tileSize = static_cast<ColorPaletteOptions::TileSize>(tileSizeIndex);
    options->setTileSize(tileSize);
    _applyTileSize(tileSize);
}

void SPDesktopWidget::toggleColorPalette() {
    _colorPaletteVisible = !_colorPaletteVisible;
    _colorPalettePanel->setVisible(dialogsDocked() && _colorPaletteVisible);
    _overlayLayout->invalidate();
    _overlayLayout->activate();
}

void SPDesktopWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls() ||
        event->mimeData()->hasFormat("application/x-inkscape-symbol")) {
        event->acceptProposedAction();
    } else {
        QWidget::dragEnterEvent(event);
    }
}

void SPDesktopWidget::dropEvent(QDropEvent* event) {
    auto const mime = event->mimeData();
    auto const is_symbol = mime->hasFormat("application/x-inkscape-symbol");
    if (!mime->hasUrls() && !is_symbol) {
        QWidget::dropEvent(event);
        return;
    }

    auto canvas = _desktop ? _desktop->getCanvas() : nullptr;
    if (!canvas) {
        event->ignore();
        return;
    }

    const QPointF canvas_pos_f = canvas->mapFromGlobal(mapToGlobal(event->position().toPoint()));
    const Geom::Point canvas_pos(canvas_pos_f.x(), canvas_pos_f.y());
    const auto world_pos = canvas->canvas_to_world(canvas_pos);
    const Geom::Point dt_pos = _desktop->w2d(world_pos);

    if (is_symbol) {
        auto const payload = mime->data("application/x-inkscape-symbol");
        QDataStream stream(payload);
        QString symbol_id;
        QString symbol_key;
        stream >> symbol_id >> symbol_key;
        if (stream.status() != QDataStream::Ok || symbol_id.isEmpty() || symbol_key.isEmpty()) {
            event->ignore();
            return;
        }

        Inkscape::UI::ClipboardManager::get()->insertSymbol(_desktop, dt_pos, false);
        Inkscape::DocumentUndo::done(_desktop->getDocument(), RC_("Undo", "Drop Symbol"), "");
        event->acceptProposedAction();
        return;
    }

    std::vector<std::string> paths;
    for (const auto& url : event->mimeData()->urls()) {
        if (!url.isLocalFile()) continue;
        paths.push_back(url.toLocalFile().toStdString());
    }

    if (paths.empty() || !ink_drop_files(_desktop, paths, dt_pos)) {
        event->ignore();
    } else {
        event->acceptProposedAction();
    }
}

} // namespace Linea::UI
