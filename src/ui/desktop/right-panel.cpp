// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * RightPanel implementation.
 */

#include "right-panel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QWidgetAction>
#include <array>

#include "desktop.h"
#include "export-widget.h"
#include "number-edit.h"
#include "qt/ui/document-properties-panel.h"
#include "qt/ui/extension-gallery.h"
#include "qt/ui/panel-switch.h"
#include "qt/ui/popup-menu.h"
#include "qt/ui/separator.h"
#include "qt/ui/stock-symbol-source.h"
#include "qt/ui/symbols-widget.h"
#include "ui/widget/toolbar.h"
#include "util/numeric/converters.h"

#include "theme.h"

namespace Linea::UI {

namespace {
constexpr int SIDE_PADDING = 5;
}

RightPanel::RightPanel(QWidget* parent)
    : CollapsiblePanel(parent) {
    setResizableEdge(Qt::LeftEdge);
    setMinimumWidth(210);
    setMaximumWidth(400);

    buildUi();
    connectSignals();
}

RightPanel::~RightPanel() = default;

void RightPanel::buildUi() {
    // --- Content widgets ---

    _propertiesPanel = new DocumentPropertiesPanel(nullptr, SIDE_PADDING, SIDE_PADDING);

    _symbolsWidget = new SymbolsWidget();
    _symbolsWidget->setContentsMargins(SIDE_PADDING, 0, SIDE_PADDING, 0);
    _symbolsSource = new StockSymbolsSource(_symbolsWidget, nullptr);

    auto content = new QWidget();
    auto contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    _panelSwitch = new PanelSwitch(content);
    _panelSwitch->setContentsMargins(SIDE_PADDING, 0, SIDE_PADDING, 4);
    contentLayout->addWidget(_panelSwitch);
    _panelSwitch->addButton("properties", {}, tr("Properties"));
    _panelSwitch->addButton("symbols", {}, tr("Symbols"));
    _panelSwitch->setCurrentIndex(0);

    auto separator = new Separator(content);
    separator->setContentsMargins(0, 0, 0, 4);
    contentLayout->addWidget(separator);

    _panelStack = new QStackedWidget(content);
    _panelStack->addWidget(_propertiesPanel);
    _panelStack->addWidget(_symbolsWidget);
    _panelStack->setCurrentIndex(0);
    contentLayout->addWidget(_panelStack);

    setContentWidget(content);
    setContentMargins(0, 0, 0, 0);
}

void RightPanel::buildHeader() {
    auto tb = new Toolbar();
    auto ext = tb->addButton("dialog-open-extension-gallery");
    auto extensionPopup = new PopupMenu();
    auto extensionGallery = new ExtensionGallery;
    extensionGallery->setFixedSize(640, 460);
    extensionPopup->setContent(extensionGallery);
    connect(ext, &QToolButton::clicked, this, [extensionPopup, ext]() {
        extensionPopup->showBelowWidget(ext);
    });
    connect(extensionGallery, &ExtensionGallery::itemActivated, extensionPopup,
            [extensionPopup](const QString&) { extensionPopup->hide(); });
    connect(extensionGallery, &ExtensionGallery::actionRequested, extensionPopup,
            [extensionPopup](const QString&) { extensionPopup->hide(); });

    tb->addStretch();
// temp --------------------------------
    auto test = tb->addPushButton("x");
    test->setToolButtonStyle(Qt::ToolButtonTextOnly);
    test->setFixedWidth(30);
    connect(test, &QToolButton::clicked, this, []() {
        static bool dark = false;
        dark = !dark;
        Linea::UI::setApplicationTheme(dark);
    });
    test->setFixedWidth(16);
// --------------------------------------

    static auto ids = std::to_array({
        "-",
        "canvas-zoom-in",
        "canvas-zoom-out",
        "canvas-zoom-1-1",
        "canvas-zoom-2-1",
        "canvas-zoom-selection",
        "canvas-zoom-drawing",
        "canvas-zoom-page",
        "canvas-zoom-page-width",
        "-",
        "canvas-display-mode-toggle",
        ">" "Pixel preview",
            "canvas-pixel-preview-toggle",
            "canvas-pixel-preview-100",
            "canvas-pixel-preview-200",
        "<",
        "-",
        "view-color-palette",
        "view-rulers",
        "view-fullscreen",
    });
    _zoomButton = tb->addMenuButton(ids, "100%");
    _zoomButton->setFixedWidth(68); // how to measure reliably?
    _zoomButton->setToolTip(tr("View options"));

    // Zoom percentage edit field at the top of the zoom popup menu.
    _zoomEdit = new NumberEdit(_zoomButton);
    _zoomEdit->setFactor(100);
    _zoomEdit->setSuffix("%");
    _zoomEdit->setDecimals(0);
    _zoomEdit->setRange(SP_DESKTOP_ZOOM_MIN, SP_DESKTOP_ZOOM_MAX);
    _zoomEdit->setValue(1.0);
    _zoomEdit->setHasFrame(true);
    // _zoomEdit->setIcon("zoom"); - doesn't look great
    auto zoomLabel = new QLabel(tr("Zoom"), _zoomButton->menu());
    zoomLabel->setProperty("class", "panel-label");
    zoomLabel->setContentsMargins(0, 0, 0, 4);
    auto zoomLabelAction = new QWidgetAction(_zoomButton->menu());
    zoomLabelAction->setDefaultWidget(zoomLabel);
    auto zoomWidgetAction = new QWidgetAction(_zoomButton->menu());
    zoomWidgetAction->setDefaultWidget(_zoomEdit);
    _zoomButton->menu()->insertAction(_zoomButton->menu()->actions().first(), zoomWidgetAction);
    _zoomButton->menu()->insertAction(zoomWidgetAction, zoomLabelAction);

    connect(_zoomEdit, &NumberEdit::valueChanged, this, [this](auto zoom) {
        Q_EMIT zoomChanged(zoom);
    });

    static auto rotation_ids = std::to_array({
        "-",
        "canvas-rotate-cw",
        "canvas-rotate-ccw",
        "canvas-rotate-reset",
        "-",
        "canvas-flip-horizontal",
        "canvas-flip-vertical",
        "canvas-flip-reset",
        // "-",
        // "canvas-rotate-lock", // not needed now
    });
    _rotationButton = tb->addMenuButton(rotation_ids, {});
    _rotationButton->setIcon(QIcon(":/icons/rotate-canvas"));
    _rotationButton->setToolTip(tr("Canvas rotation"));
    _rotationEdit = new NumberEdit(_rotationButton);
    _rotationEdit->setSuffix("°");
    _rotationEdit->setDecimals(0);
    _rotationEdit->setRange(-179, 180);
    _rotationEdit->setValue(0);
    _rotationEdit->setWrapping(true);
    _rotationEdit->setHasFrame(true);
    auto rotationLabel = new QLabel(tr("Orientation"), _rotationButton->menu());
    rotationLabel->setProperty("class", "panel-label");
    rotationLabel->setContentsMargins(0, 0, 0, 4);
    auto rotationLabelAction = new QWidgetAction(_rotationButton->menu());
    rotationLabelAction->setDefaultWidget(rotationLabel);

    auto rotationWidgetAction = new QWidgetAction(_rotationButton->menu());
    rotationWidgetAction->setDefaultWidget(_rotationEdit);
    _rotationButton->menu()->insertAction(_rotationButton->menu()->actions().first(), rotationWidgetAction);
    _rotationButton->menu()->insertAction(rotationWidgetAction, rotationLabelAction);

    connect(_rotationEdit, &NumberEdit::valueChanged, this, [this](auto rotation) {
        Q_EMIT rotationChanged(rotation);
    });

    static auto snap_ids = std::to_array({"simple-snap-bbox", "simple-snap-nodes", "simple-snap-alignment"});
    tb->addSplitMenuButton(snap_ids, "snap-global-toggle");
    tb->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setHeader(tb);
}

void RightPanel::connectSignals() {
    connect(_panelSwitch, &PanelSwitch::currentChanged, _panelStack, &QStackedWidget::setCurrentIndex);
}

void RightPanel::setDesktop(SPDesktop* desktop) {
    if (!desktop) {
        clear();
        return;
    }
    _propertiesPanel->setSelection(desktop);
}

void RightPanel::clear() {
    _propertiesPanel->setSelection(nullptr);
}

void RightPanel::saveSettings(QSettings& settings) {
    settings.setValue("rightPanelWidth", width());
}

void RightPanel::restoreSettings(QSettings& settings) {
    int w = settings.value("rightPanelWidth", 300).toInt();
    resize(w, height());
}

void RightPanel::updateZoom(double zoom) {
    // update zoom status in the UI
    auto percent = zoom * 100;
    int precision = 0;
    // this might be an overkill... zoom edit box doesn't allow decimal places
    if (percent < 10) {
        precision = 2;
    } else if (percent < 100) {
        precision = 1;
    }
    _zoomButton->setText(" " + Inkscape::Util::formatTrimmed(percent, precision) + "%");
    if (_zoomEdit) {
        const QSignalBlocker blocker(_zoomEdit);
        _zoomEdit->setValue(zoom);
    }
}

void RightPanel::updateRotation(double angle) {
    // update rotation status in the UI
    const QSignalBlocker blocker(_rotationEdit);
    _rotationEdit->setValue(angle);
}

} // namespace Linea::UI
