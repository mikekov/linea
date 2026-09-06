// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Document Properties Panel for editing document element properties.
 *
 */

#ifndef LINEA_UI_DOCUMENTPROPERTIESPANEL_H
#define LINEA_UI_DOCUMENTPROPERTIESPANEL_H

#include <QWidget>
#include <sigc++/connection.h>
#include <sigc++/scoped_connection.h>
#include <memory>
#include "appearance-panel.h"
#include "dynamic-stacked-widget.h"
#include "display-widget.h"
#include "document-widget.h"
#include "export-widget.h"
#include "image-widget.h"
#include "node-widget.h"
#include "pen-widget.h"
#include "pencil-widget.h"
#include "calligraphic-widget.h"
#include "eraser-widget.h"
#include "tweak-widget.h"
#include "spray-widget.h"
#include "paintbucket-widget.h"
#include "connector-widget.h"
#include "select-widget.h"
#include "qt/util/virtual-node-type.h"
#include "zoom-widget.h"

namespace Linea::UI {
class SizeWidget;
}
#include "size-widget.h"

QT_BEGIN_NAMESPACE
class QStackedWidget;
QT_END_NAMESPACE

namespace Inkscape {
class Selection;
}

class SPDesktop;
class SPDocument;
class SPNamedView;

namespace Linea {

namespace Props {
class Binder;
class Editor;
}

namespace UI {

class MetadataPanel;
class LicensePanel;
class GridsPanel;
class GuidesPanel;
class PagesPanel;
class PopupMenu;
class ExportWidget;
class Separator;

/**
 * Document Properties Panel for editing selected object properties.
 *
 * This widget provides a comprehensive interface for editing properties of
 * selected objects in the Inkscape document.
 */
class DocumentPropertiesPanel : public QWidget {
    Q_OBJECT

public:
    explicit DocumentPropertiesPanel(QWidget* parent = nullptr, int paddingLeft = 0, int paddingRight = 0);
    ~DocumentPropertiesPanel() override;

    // Selection management
    void setSelection(SPDesktop* desktop);
    Inkscape::Selection* selection() const { return _selection; }

    void showProperties(Linea::UI::VirtualNodeType node);

    // Show metadata panel for document (when "About" virtual item is selected)
    void showDocumentMetadata(SPDocument* document);

    // Show grids panel for the document
    void showGrids(SPNamedView* namedview);

    // Show guides panel for the document
    void showGuides(SPNamedView* namedview);

    // Show object properties panel
    void showObjectProperties();

    void showDocumentProperties(SPDocument* document);
    void showDisplayProperties(SPDocument* document);
    void showPages(SPDocument* document);

    // Prevent copying
    DocumentPropertiesPanel(const DocumentPropertiesPanel&) = delete;
    DocumentPropertiesPanel& operator=(const DocumentPropertiesPanel&) = delete;

public Q_SLOTS:
    // Selection change handlers
    void onSelectionChanged();

private:
    // Data
    Inkscape::Selection* _selection = nullptr;
    SPDesktop* _desktop = nullptr;
    SPDocument* _document = nullptr;
    OperationBlocker _updatingFromSelection;

    // UI
    DynamicSizeStackedWidget* _stackedWidget = nullptr;

    // Those panels are driven by "virtual" nodes in object tree panel
    QWidget* _aboutPanel = nullptr;      // Combined metadata + license panel
    MetadataPanel* _metadataPanel = nullptr;
    LicensePanel* _licensePanel = nullptr;
    // grid definition panel
    GridsPanel* _gridsPanel = nullptr;
    GuidesPanel* _guidesPanel = nullptr;
    // document properties (front page, viewport)
    DocumentWidget* _documentWidget = nullptr;
    // display properties (colors, rendering)
    DisplayWidget* _displayWidget = nullptr;
    // document pages
    PagesPanel* _pagesPanel = nullptr;
    // -----------------------------------------
    // document object properties
    DynamicSizeStackedWidget* _toolWidgetStack = nullptr;
    NodeWidget* _nodeWidget = nullptr;
    PenWidget* _penWidget = nullptr;
    PencilWidget* _pencilWidget = nullptr;
    CalligraphicWidget* _calligraphicWidget = nullptr;
    EraserWidget* _eraserWidget = nullptr;
    TweakWidget* _tweakWidget = nullptr;
    SprayWidget* _sprayWidget = nullptr;
    PaintbucketWidget* _paintbucketWidget = nullptr;
    ConnectorWidget* _connectorWidget = nullptr;
    SelectWidget* _selectWidget = nullptr;
    ZoomWidget* _zoomWidget = nullptr;
    AppearancePanel* _appearancePanel = nullptr;
    Separator* _exportSeparator = nullptr;
    ExportWidget* _exportWidget = nullptr;
    QWidget* _containerWidget = nullptr;

    // property system (per-desktop); widgets are re-bound on desktop change.
    std::unique_ptr<Props::Editor> _editor;
    std::unique_ptr<Props::Binder> _binder;

    // Signal connections
    sigc::scoped_connection _selectionChangedConnection;
};

} // namespace UI
} // namespace Linea

#endif // LINEA_UI_DOCUMENTPROPERTIESPANEL_H
