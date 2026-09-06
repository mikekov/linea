// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * LeftPanel implementation.
 */

#include "left-panel.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QSettings>
#include <QStackedWidget>
#include <QTreeWidget>

#include "desktop.h"
#include "document.h"
#include "document-undo.h"
#include "layer-manager.h"
#include "object-tree-model.h"
#include "object-tree-toolbar.h"
#include "object-tree-view.h"
#include "object/sp-item-group.h"
#include "object/sp-item.h"
#include "object/sp-namedview.h"
#include "qt/ui/layer-selector.h"
#include "qt/ui/panel-switch.h"
#include "qt/ui/separator.h"
#include "qt/ui/xml-tree-widget.h"
#include "selection.h"
#include "ui/widget/toolbar.h"
#include "util-string/context-string.h"

#include "document-templates-menu.h"
#include "io/recent-files.h"
#include "linea-application.h"
#include "preferences.h"
#include "ui/widget/custom-menu.h"

#include <giomm.h>

namespace Linea::UI {

namespace {

constexpr int SIDE_PADDING = 5;

bool isRealLayer(const SPObject* object) {
    auto group = cast<SPGroup>(object);
    return group && group->layerMode() == SPGroup::LAYER;
}

void set_dt_select(Inkscape::XML::Node* repr, SPDesktop* desktop) {
    auto document = desktop->getDocument();
    if (!document) return;

    SPObject* object = nullptr;
    if (repr) {
        while (repr->type() != Inkscape::XML::NodeType::ELEMENT_NODE && repr->parent()) {
            repr = repr->parent();
        }
        object = document->getObjectByRepr(repr);
    }

    if (!object) {
        // object not on canvas
    } else if (isRealLayer(object)) {
        desktop->layerManager().setCurrentLayer(object);
    } else {
        desktop->getSelection()->set(object);
    }
}

std::vector<CustomMenuItem> recentFilesMenu() {
    int max_files = Inkscape::Preferences::get()->getInt("/options/maxrecentdocuments/value", 20);
    if (max_files <= 0) return {};

    if (max_files > 20) {
        max_files = 20; //TODO: find max
    }

    auto recent = Linea::IO::getRecentFiles(max_files);
    if (recent.empty()) return {};

    auto shortened = Linea::IO::getShortenedPathMap(recent);

    std::vector<CustomMenuItem> items;
    items.reserve(recent.size());
    for (auto& rf : recent) {
        auto label = QString::fromStdString(shortened[rf.path]);
        if (label.isEmpty()) label = QString::fromStdString(rf.display_name);

        auto path = rf.path;
        items.push_back(CustomMenuItem{
            .title = label,
            .description = QString::fromStdString(rf.path),
            .onTrigger = [path = std::move(path)] {
                auto file = Gio::File::create_for_path(path);
                LINEA_APP.openDocument(file);
            },
        });
    }
    return items;
}

} // namespace

LeftPanel::LeftPanel(QWidget* parent)
    : CollapsiblePanel(parent) {
    setResizableEdge(Qt::RightEdge);
    setContentMargins(6, 0, 6, 6);
    setMinimumWidth(210);
    setMaximumWidth(400);

    buildUi();
    connectSignals();
}

LeftPanel::~LeftPanel() = default;

void LeftPanel::buildUi() {
    // --- Content widgets ---

    _stackedWidget = new QStackedWidget();
    _xmlTreeWidget = new XmlTreeWidget();
    _xmlTreeWidget->setProperty("class", "widget-frame");

    // Object tree container with toolbar
    _objectTreeContainer = new QWidget();
    auto objectTreeLayout = new QVBoxLayout(_objectTreeContainer);
    objectTreeLayout->setContentsMargins(0, 0, 0, 0);
    objectTreeLayout->setSpacing(4);

    _objectTreeToolbar = new ObjectTreeToolbar();
    objectTreeLayout->addWidget(_objectTreeToolbar);

    _objectTreeView = new ObjectTreeView();
    _objectTreeView->setProperty("class", "widget-frame");
    objectTreeLayout->addWidget(_objectTreeView);

    _layerSelector = _objectTreeToolbar->getLayerSelector();

    _objectTreeToolbar->connectToView(_objectTreeView);

    _stackedWidget->addWidget(_objectTreeContainer);
    _objectTreeContainer->setContentsMargins(SIDE_PADDING, 0, SIDE_PADDING, 0);
    _stackedWidget->addWidget(_xmlTreeWidget);
    _xmlTreeWidget->setContentsMargins(SIDE_PADDING, 0, SIDE_PADDING, 0);

    // --- Panel content: PanelSwitch + search + Separator + stack ---

    auto content = new QWidget();
    auto contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    // Panel switch + search box in a horizontal row.
    auto switchRow = new QHBoxLayout();
    switchRow->setContentsMargins(SIDE_PADDING, 0, SIDE_PADDING, 4);
    switchRow->setSpacing(4);

    _panelSwitch = new PanelSwitch(content);
    _panelSwitch->setContentsMargins(0, 0, 0, 0);
    switchRow->addWidget(_panelSwitch);
    _panelSwitch->addButton("dialog-objects", {}, tr("Objects"));
    _panelSwitch->addButton("document-metadata", {}, tr("XML Tree"));
    _panelSwitch->setCurrentIndex(0);

    switchRow->addStretch();

    _searchEdit = new QLineEdit(content);
    _searchEdit->setPlaceholderText(tr("Search\u2026"));
    _searchEdit->setClearButtonEnabled(true);
    _searchEdit->setMaximumWidth(120);
    _searchEdit->setToolTip(tr("Filter tree contents"));
    switchRow->addWidget(_searchEdit);

    contentLayout->addLayout(switchRow);

    auto separator = new Separator(content);
    separator->setContentsMargins(0, 0, 0, 4);
    contentLayout->addWidget(separator);

    contentLayout->addWidget(_stackedWidget);

    setContentWidget(content);
    setContentMargins(0, 0, 0, 0);
    _stackedWidget->setCurrentIndex(0);
}

void LeftPanel::buildHeader() {
    auto tb = new Toolbar();
    tb->addButton("document-new");
    tb->addDropDownButton(newDocumentFromTemplateMenu(), {});
    tb->addSpace();
    tb->addButton("document-open");
    tb->addDynamicDropDownButton(recentFilesMenu);
    tb->addSpace();
    tb->addButton("document-save");
    tb->addStretch();
    tb->addButton("toggle-panel-docking");
    tb->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    setHeader(tb);
}

void LeftPanel::connectSignals() {
    connect(_panelSwitch, &PanelSwitch::currentChanged, _stackedWidget, &QStackedWidget::setCurrentIndex);
    connect(_panelSwitch, &PanelSwitch::currentChanged, this, &LeftPanel::panelSwitchChanged);

    // Search box filters both object tree and XML tree.
    connect(_searchEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        if (_objectTreeView) {
            _objectTreeView->setFilterText(text);
            if (_desktop) {
                _objectTreeView->selectObjects(_desktop->getSelection()->objects());
            }
        }
        if (_xmlTreeWidget) _xmlTreeWidget->setFilterText(text);
    });

    // Layer selector
    connect(_layerSelector, &LayerSelector::populateLayers, this, [this](QTreeWidget* tree) {
        if (!_desktop) return;
        auto& lm = _desktop->layerManager();
        populateLayerTree(tree, lm.currentRoot(), lm.currentLayer());
    });

    connect(_layerSelector, &LayerSelector::layerSelected, this, [this](const QString& id) {
        if (!_desktop) return;
        if (auto obj = _desktop->getDocument()->getObjectById(id.toStdString())) {
            _desktop->layerManager().setCurrentLayer(obj);
            _objectTreeView->collapseAllExcept(obj);
        }
    });

    // Object tree selection
    connect(_objectTreeView, &ObjectTreeView::objectsSelected, this, [this](std::vector<SPObject*> objects) {
        if (!_desktop || _syncObjectTree.pending()) return;
        auto guard = _syncObjectTree.block();
        _desktop->setSelectedVirtualNode(VirtualNodeType::None);
        if (objects.empty()) {
            _desktop->getSelection()->clear();
        } else {
            if (objects.size() == 1) {
                if (auto layer = cast<SPGroup>(objects.front()); layer != nullptr && layer->isLayer()) {
                    _desktop->layerManager().setCurrentLayer(layer);
                }
                _desktop->getSelection()->set(objects.front());
            } else {
                _desktop->getSelection()->setList(objects);
            }
        }
        Q_EMIT objectsSelected(objects);
    });

    // Virtual node selection
    connect(_objectTreeView, &ObjectTreeView::virtualNodeSelected, this, [this](VirtualNodeType type) {
        if (!_desktop) return;
        auto guard = _syncObjectTree.block();
        _desktop->getSelection()->clear();
        _desktop->setSelectedVirtualNode(type);
        Q_EMIT virtualNodeSelected(static_cast<int>(type));
    });

    // Object visibility/lock toggles
    connect(_objectTreeView, &ObjectTreeView::objectVisibilityRequested, this, [this](SPObject* object, bool hidden) {
        if (!_desktop) return;
        auto item = cast<SPItem>(object);
        if (!item) return;
        item->setHidden(hidden);
        auto msg = hidden ? RC_("Undo", "Hide object") : RC_("Undo", "Show object");
        Inkscape::DocumentUndo::done(_desktop->getDocument(), msg, "");
    });

    connect(_objectTreeView, &ObjectTreeView::objectLockRequested, this, [this](SPObject* object, bool lock) {
        if (!_desktop) return;
        auto item = cast<SPItem>(object);
        if (!item) return;
        item->setLocked(lock);
        auto msg = lock ? RC_("Undo", "Lock object") : RC_("Undo", "Unlock object");
        Inkscape::DocumentUndo::done(_desktop->getDocument(), msg, "");
    });

    connect(_objectTreeView, &ObjectTreeView::virtualNodeVisibilityRequested, this,
            [this](VirtualNodeType type, bool hidden) {
                if (!_desktop) return;
                auto nv = _desktop->getNamedView();
                if (!nv) return;
                switch (type) {
                    case VirtualNodeType::Guides:
                        nv->setShowGuides(!hidden);
                        break;
                    case VirtualNodeType::Grids:
                        nv->setShowGrids(!hidden);
                        break;
                    default:
                        return;
                }
            });

    connect(_objectTreeView, &ObjectTreeView::virtualNodeLockRequested, this, [this](VirtualNodeType type, bool lock) {
        if (!_desktop) return;
        auto nv = _desktop->getNamedView();
        if (!nv || type != VirtualNodeType::Guides) return;
        nv->setLockGuides(lock);
        auto msg = lock ? RC_("Undo", "Lock guides") : RC_("Undo", "Unlock guides");
        Inkscape::DocumentUndo::done(_desktop->getDocument(), msg, "");
    });

    connect(_objectTreeView, &ObjectTreeView::objectLabelRequested, this,
            [this](SPObject* object, const QString& label) {
                if (!_desktop || !object) return;
                object->setLabel(label.toUtf8().constData());
                Inkscape::DocumentUndo::done(_desktop->getDocument(), RC_("Undo", "Rename object"), "");
            });

    // XML tree selection
    connect(_xmlTreeWidget, &XmlTreeWidget::nodeSelected, this, [this](Inkscape::XML::Node* node) {
        if (!_desktop || _syncXmlTree.pending()) return;
        auto guard = _syncXmlTree.block();
        set_dt_select(node, _desktop);
        Q_EMIT xmlNodeSelected(node);
    });
}

void LeftPanel::setDesktop(SPDesktop* desktop) {
    if (!desktop) {
        clear();
        return;
    }

    _desktop = desktop;

    _xmlTreeWidget->setDesktop(desktop);
    _xmlTreeWidget->setDocument(desktop->doc());
    _objectTreeView->setDesktop(desktop);
    _objectTreeView->buildTree(desktop->document());

    // Update current layer in object tree view when it changes
    auto updateLayerButton = [this](SPObject* layer) {
        auto label = layer ? layer->label() : nullptr;
        _layerSelector->setCurrentLayer(QString::fromUtf8(label ? label : "-"));
    };
    auto currentLayer = desktop->layerManager().currentLayer();
    _objectTreeView->setCurrentLayer(currentLayer);
    updateLayerButton(currentLayer);
    _layerChangedConn = desktop->layerManager().connectCurrentLayerChanged(
        [this, updateLayerButton](SPGroup* layer) {
            _objectTreeView->setCurrentLayer(layer);
            updateLayerButton(layer);
            _currentLayerModifiedConn.disconnect();
            if (layer) {
                _currentLayerModifiedConn = layer->connectModified(
                    [this, updateLayerButton](SPObject*, unsigned) {
                        if (_desktop) {
                            updateLayerButton(_desktop->layerManager().currentLayer());
                        }
                    });
            }
        });

    // Restore the saved virtual node selection or the tree's selection from
    // the desktop's current object selection.
    auto virtualNode = desktop->selectedVirtualNode();
    if (virtualNode != VirtualNodeType::None) {
        _objectTreeView->restoreVirtualNode(virtualNode);
    } else {
        _objectTreeView->restoreSelection(desktop->getSelection()->objects());
    }
}

void LeftPanel::clear() {
    _layerChangedConn.disconnect();
    _currentLayerModifiedConn.disconnect();

    _xmlTreeWidget->setDocument(nullptr);
    _objectTreeView->buildTree(nullptr);
    _layerSelector->setCurrentLayer({});
    _desktop = nullptr;
}

void LeftPanel::updateSelectionFromDesktop() {
    if (!_desktop) return;

    auto sel = _desktop->getSelection();
    if (!_syncObjectTree.pending()) {
        auto guard = _syncObjectTree.block();
        _objectTreeView->selectObjects(sel->objects());
    }
    if (!_syncXmlTree.pending()) {
        auto guard = _syncXmlTree.block();
        SPObject* obj = sel && !sel->objects().empty() ? sel->objects().front() : nullptr;
        _xmlTreeWidget->setSelectedNode(obj ? obj->getRepr() : nullptr);
    }
}

void LeftPanel::saveSettings(QSettings& settings) {
    settings.setValue("leftPanelWidth", width());
}

void LeftPanel::restoreSettings(QSettings& settings) {
    int w = settings.value("leftPanelWidth", 300).toInt();
    resize(w, height());
}

} // namespace Linea::UI
