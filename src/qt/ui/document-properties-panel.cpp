// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Document Properties Panel implementation.
 */

#include "document-properties-panel.h"

#include <QDoubleSpinBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "appearance-panel.h"
#include "desktop.h"
#include "document.h"
#include "grids-panel.h"
#include "guides-panel.h"
#include "pages-panel.h"
#include "license-panel.h"
#include "metadata-panel.h"
#include "selection.h"
#include "separator.h"
#include "util/object-modified-tags.h"
#include "widget-utils.h"
#include "props/binder.h"
#include "props/conditions.h"
#include "props/editor.h"

namespace Linea::UI {

const auto TAG = get_next_object_modified_tag();

using NumberEdit = Linea::UI::NumberEdit;

DocumentPropertiesPanel::DocumentPropertiesPanel(QWidget* parent, int paddingLeft, int paddingRight)
    : QWidget(parent)
    , _selection(nullptr) {
    // Create stacked widget as the main container
    _stackedWidget = new DynamicSizeStackedWidget(this);

    // Wrap the entire stacked widget in a single scroll area
    auto scrollArea = new QScrollArea(this);
    scrollArea->setWidget(_stackedWidget);
    scrollArea->setWidgetResizable(true);
    scrollArea->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    // Create a container widget for the UI content
    _containerWidget = new QWidget(this);
    _containerWidget->setMinimumWidth(0);
    _containerWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::MinimumExpanding);
    auto vbox = new QVBoxLayout();
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);
    _containerWidget->setLayout(vbox);

    _selectWidget = new SelectWidget(this);
    _penWidget = new PenWidget(this);
    _pencilWidget = new PencilWidget(this);
    _calligraphicWidget = new CalligraphicWidget(this);
    _eraserWidget = new EraserWidget(this);
    _tweakWidget = new TweakWidget(this);
    _sprayWidget = new SprayWidget(this);
    _paintbucketWidget = new PaintbucketWidget(this);
    _connectorWidget = new ConnectorWidget(this);
    _zoomWidget = new ZoomWidget(this);
    _nodeWidget = new NodeWidget(this);

    // Tool-option panels live in a stacked widget so switching tools swaps
    // the current page instead of show/hide reflowing the container layout.
    _toolWidgetStack = new DynamicSizeStackedWidget(this);
    _toolWidgetStack->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    _toolWidgetStack->addWidget(_selectWidget);
    _toolWidgetStack->addWidget(_penWidget);
    _toolWidgetStack->addWidget(_pencilWidget);
    _toolWidgetStack->addWidget(_calligraphicWidget);
    _toolWidgetStack->addWidget(_eraserWidget);
    _toolWidgetStack->addWidget(_tweakWidget);
    _toolWidgetStack->addWidget(_sprayWidget);
    _toolWidgetStack->addWidget(_paintbucketWidget);
    _toolWidgetStack->addWidget(_connectorWidget);
    _toolWidgetStack->addWidget(_zoomWidget);
    _toolWidgetStack->addWidget(_nodeWidget);
    vbox->addWidget(_toolWidgetStack);

    auto toolGap = new QWidget(this);
    toolGap->setFixedHeight(4);
    toolGap->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    vbox->addWidget(toolGap);

    _appearancePanel = new AppearancePanel(this, paddingLeft, paddingRight);
    vbox->addWidget(_appearancePanel);

    _exportSeparator = new Separator(this);
    _exportWidget = new ExportWidget(this);
    _exportWidget->layout()->setContentsMargins(paddingLeft, 4, paddingRight, 0);
    vbox->addWidget(_exportSeparator);
    vbox->addWidget(_exportWidget);

    // Create combined "About" panel with metadata and license
    _aboutPanel = new QWidget(this);
    _aboutPanel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto aboutLayout = new QVBoxLayout(_aboutPanel);
    aboutLayout->setContentsMargins(0, 0, 0, 0);
    aboutLayout->setSpacing(4);

    auto aboutLabel = new QLabel(tr("About Document"));
    aboutLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    aboutLabel->setProperty("class", "panel-heading");
    aboutLayout->addWidget(aboutLabel);

    // Add metadata panel
    _metadataPanel = new MetadataPanel(false, _aboutPanel);
    aboutLayout->addWidget(_metadataPanel, 1);

    // Add license panel
    _licensePanel = new LicensePanel(_aboutPanel);
    aboutLayout->addWidget(_licensePanel);

    // page size, coordinate system
    _documentWidget = new DocumentWidget();
    _stackedWidget->addWidget(_documentWidget);

    // decorators, display units, etc.
    _displayWidget = new DisplayWidget();
    _stackedWidget->addWidget(_displayWidget);

    // TODO: measure columns in both panels and make them equal
    //  shortcut for now
    _metadataPanel->gridLayout()->setColumnMinimumWidth(0, 80);
    _licensePanel->gridLayout()->setColumnMinimumWidth(0, 80);

    // Create grids panel
    _gridsPanel = new GridsPanel(this);

    // Create guides panel
    _guidesPanel = new GuidesPanel(this);

    // Create pages panel
    _pagesPanel = new PagesPanel(this);

    // Add panels to the stacked widget
    _stackedWidget->addWidget(_containerWidget);
    _stackedWidget->addWidget(_aboutPanel);
    _stackedWidget->addWidget(_gridsPanel);
    _stackedWidget->addWidget(_guidesPanel);
    _stackedWidget->addWidget(_pagesPanel);

    // Apply left/right padding as content margins to the child panels.
    // The appearance panel is skipped: it receives the padding values in its
    // constructor and manages its own margins.
    for (auto widget : std::initializer_list<QWidget*>{
        _selectWidget, _penWidget, _pencilWidget, _calligraphicWidget,
        _eraserWidget, _tweakWidget, _sprayWidget, _paintbucketWidget,
        _connectorWidget, _zoomWidget, _nodeWidget,
        _aboutPanel, _documentWidget, _displayWidget,
        _gridsPanel, _guidesPanel, _pagesPanel,
    }) {
        applyHorizontalPadding(widget, paddingLeft, paddingRight);
    }

    // Set up main layout
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(scrollArea);

    // Show object properties by default
    _stackedWidget->setCurrentWidget(_containerWidget);

    vbox->addStretch();

    _stackedWidget->hide();
    // showObjectProperties();
}

DocumentPropertiesPanel::~DocumentPropertiesPanel() = default;

void DocumentPropertiesPanel::setSelection(SPDesktop* desktop) {
    auto selection = desktop ? desktop->getSelection() : nullptr;
    auto document = selection ? selection->document() : nullptr;
    if (_selection == selection && _desktop == desktop && _document == document) {
        return;
    }

    _selectionChangedConnection.disconnect();
    _selection = selection;
    _desktop = desktop;
    _document = document;
    _stackedWidget->setVisible(_document != nullptr);

    if (!_selection) {
        _appearancePanel->setDesktop(nullptr, nullptr, nullptr);
        _metadataPanel->setDocument(nullptr);
        _licensePanel->setDocument(nullptr);
        _documentWidget->setDocument(nullptr);
        _documentWidget->setDesktop(nullptr);
        _displayWidget->setDocument(nullptr);
        _gridsPanel->setNamedview(nullptr);
        _gridsPanel->update(nullptr);
        _guidesPanel->setNamedview(nullptr);
        _guidesPanel->update(nullptr);
        _pagesPanel->setDesktop(nullptr);
        _pagesPanel->setDocument(nullptr);
        _pagesPanel->update(nullptr);
    }

    // New property system: rebuild the editor/binder for this desktop.
    // Binder teardown severs the widget connections made by bind() below.
    _binder.reset();
    _editor.reset();

    _selectWidget->setDesktop(desktop);
    _penWidget->setDesktop(desktop);
    _pencilWidget->setDesktop(desktop);
    _calligraphicWidget->setDesktop(desktop);
    _eraserWidget->setDesktop(desktop);
    _tweakWidget->setDesktop(desktop);
    _sprayWidget->setDesktop(desktop);
    _paintbucketWidget->setDesktop(desktop);
    _connectorWidget->setDesktop(desktop);
    _nodeWidget->setDesktop(desktop);

    if (desktop) {
        _editor = std::make_unique<Props::Editor>(desktop, TAG);
        _editor->setTargetScope(desktop->stateModel()->targetScope());
        _binder = std::make_unique<Props::Binder>(desktop->stateModel(), _editor.get());

        _selectWidget->bind(*_binder);
        _nodeWidget->bind(*_binder);

        // Tool-option panel visibility is driven declaratively by the binder:
        // the stack switches to the first matching page (blank fallback when
        // no tool panel applies), hiding the stack when nothing matches.
        using namespace Props::Cond;
        _binder->switchTo(_toolWidgetStack, {
            {toolIs<TOOLS_SELECT>,          _selectWidget},
            {toolIs<TOOLS_FREEHAND_PEN>,    _penWidget},
            {toolIs<TOOLS_FREEHAND_PENCIL>, _pencilWidget},
            {toolIs<TOOLS_CALLIGRAPHIC>,    _calligraphicWidget},
            {toolIs<TOOLS_ERASER>,          _eraserWidget},
            {toolIs<TOOLS_TWEAK>,           _tweakWidget},
            {toolIs<TOOLS_SPRAY>,           _sprayWidget},
            {toolIs<TOOLS_PAINTBUCKET>,     _paintbucketWidget},
            {toolIs<TOOLS_CONNECTOR>,       _connectorWidget},
            {toolIs<TOOLS_NODES>,           _nodeWidget},
            {toolIs<TOOLS_ZOOM>,            _zoomWidget},
        });

        _selectionChangedConnection = _selection->connectChanged([this](auto selection) { onSelectionChanged(); });

        _appearancePanel->setDesktop(desktop, _document, _binder.get());

        _exportWidget->bind(*_binder);
        syncVisibility(_exportSeparator, _exportWidget);
    }
}

void DocumentPropertiesPanel::onSelectionChanged() {
    if (!_selection || _updatingFromSelection.pending()) return;

    showObjectProperties();
}

void DocumentPropertiesPanel::showProperties(Linea::UI::VirtualNodeType node) {
    if (!_desktop) return;

    auto scoped(_updatingFromSelection.block());

    switch (node) {
        case Linea::UI::VirtualNodeType::DocumentProps:
            showDocumentProperties(_desktop->getDocument());
            break;
        case Linea::UI::VirtualNodeType::Display:
            showDisplayProperties(_desktop->getDocument());
            break;
        case Linea::UI::VirtualNodeType::Pages:
            showPages(_desktop->getDocument());
            break;
        case Linea::UI::VirtualNodeType::About:
            showDocumentMetadata(_desktop->getDocument());
            break;
        case Linea::UI::VirtualNodeType::Grids:
            showGrids(_desktop->getNamedView());
            break;
        case Linea::UI::VirtualNodeType::Guides:
            showGuides(_desktop->getNamedView());
            break;
        default:
            showObjectProperties();
            break;
    }
}

void DocumentPropertiesPanel::showDocumentMetadata(SPDocument* document) {
    _metadataPanel->setDocument(document);
    _metadataPanel->update(document);
    _licensePanel->setDocument(document);
    _licensePanel->update(document);
    _stackedWidget->setCurrentWidget(_aboutPanel);
}

void DocumentPropertiesPanel::showGrids(SPNamedView* namedview) {
    _gridsPanel->setNamedview(namedview);
    _gridsPanel->update(namedview);
    _stackedWidget->setCurrentWidget(_gridsPanel);
}

void DocumentPropertiesPanel::showGuides(SPNamedView* namedview) {
    _guidesPanel->setNamedview(namedview);
    _guidesPanel->update(namedview);
    _stackedWidget->setCurrentWidget(_guidesPanel);
}

void DocumentPropertiesPanel::showObjectProperties() {
    _stackedWidget->setCurrentWidget(_containerWidget);
}

void DocumentPropertiesPanel::showDocumentProperties(SPDocument* document) {
    _documentWidget->setDocument(document);
    _documentWidget->setDesktop(_desktop);
    _documentWidget->update(document->getNamedView(), document->getRoot());
    _stackedWidget->setCurrentWidget(_documentWidget);
}

void DocumentPropertiesPanel::showDisplayProperties(SPDocument* document) {
    _displayWidget->setDocument(document);
    _displayWidget->update(document->getNamedView());
    _stackedWidget->setCurrentWidget(_displayWidget);
}

void DocumentPropertiesPanel::showPages(SPDocument* document) {
    _pagesPanel->setDocument(document);
    _pagesPanel->setDesktop(_desktop);
    _pagesPanel->update(document);
    _stackedWidget->setCurrentWidget(_pagesPanel);
}

} // namespace Linea::UI
