// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * LayerSelector implementation — Qt widget for selecting the current layer.
 */

#include "layer-selector.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <glibmm/i18n.h>

#include "eliding-label.h"
#include "icon-widget.h"
#include "object/sp-item-group.h"
#include "object/sp-object.h"
#include "popup-menu.h"
#include "popup-tree-helper.h"

namespace Linea::UI {

namespace {

constexpr int IdRole = Qt::UserRole;

void buildLayerTreeRecursive(QTreeWidget* tree, QTreeWidgetItem* parentItem, SPObject* obj, SPObject* currentLayer) {
    for (auto& child : obj->children) {
        if (!SP_IS_LAYER(&child)) continue;

        auto item = new QTreeWidgetItem(); // parentItem);
        auto label = child.label();
        item->setText(0, QString::fromUtf8(label ? label : ""));
        auto id = child.getId();
        item->setData(0, IdRole, QString::fromUtf8(id ? id : ""));
        parentItem->insertChild(0, item);

        if (&child == currentLayer) {
            tree->setCurrentItem(item);
            for (auto p = item->parent(); p; p = p->parent()) {
                p->setExpanded(true);
            }
        }

        buildLayerTreeRecursive(tree, item, &child, currentLayer);
    }
}

} // namespace

void populateLayerTree(QTreeWidget* tree, SPObject* currentRoot, SPObject* currentLayer) {
    if (!tree || !currentRoot) return;

    tree->clear();
    buildLayerTreeRecursive(tree, tree->invisibleRootItem(), currentRoot, currentLayer);
}

LayerSelector::LayerSelector(QWidget* parent)
    : QWidget(parent) {
    auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    _button = new QPushButton(this);
    _button->setToolTip(QString::fromUtf8(_("Current layer")));
    auto arrow = new IconWidget(QIcon(":/icons/pan-down"), _button);
    _label = new ElidingLabel(_button);
    auto btnLayout = new QHBoxLayout(_button);
    btnLayout->setContentsMargins(6, 0, 6, 0);
    btnLayout->addWidget(_label);
    btnLayout->addWidget(arrow);
    connect(_button, &QPushButton::clicked, this, &LayerSelector::onButtonClicked);
    layout->addWidget(_button);
}

LayerSelector::~LayerSelector() = default;

void LayerSelector::setCurrentLayer(const QString& id) {
    _label->setFullText(id);
}

void LayerSelector::onButtonClicked() {
    if (!_popup) buildPopup();
    Q_EMIT populateLayers(_tree);
    _searchEdit->clear();
    _popup->showBelowWidget(_button);
    _searchEdit->setFocus();
}

void LayerSelector::buildPopup() {
    _popup = new PopupMenu(this);

    auto content = new QWidget();
    auto contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(4);

    _searchEdit = new QLineEdit(content);
    _searchEdit->setPlaceholderText(QString::fromUtf8(_("Search...")));
    contentLayout->addWidget(_searchEdit);

    _tree = new QTreeWidget(content);
    _tree->setFixedSize(240, 280);
    _tree->setHeaderHidden(true);
    _tree->setRootIsDecorated(true);
    _tree->setExpandsOnDoubleClick(false);
    _tree->setUniformRowHeights(true);
    _tree->setFocusPolicy(Qt::NoFocus);
    contentLayout->addWidget(_tree);

    connect(_searchEdit, &QLineEdit::textChanged, this, &LayerSelector::onSearchChanged);
    connect(_tree, &QTreeWidget::itemClicked, this, &LayerSelector::onItemClicked);

    _searchEdit->installEventFilter(this);

    _popup->setContent(content);
}

void LayerSelector::onSearchChanged() {
    filterTree(_searchEdit->text().toLower());
    auto first = firstVisibleItem(_tree);
    if (first) {
        _tree->setCurrentItem(first);
    }
}

void LayerSelector::filterTree(const QString& text) {
    for (int i = 0; i < _tree->topLevelItemCount(); ++i) {
        filterItem(_tree->topLevelItem(i), text);
    }
}

bool LayerSelector::filterItem(QTreeWidgetItem* item, const QString& text) {
    bool isLeaf = item->childCount() == 0;

    if (isLeaf) {
        bool match = text.isEmpty() || item->text(0).toLower().contains(text);
        item->setHidden(!match);
        return match;
    }

    bool anyVisible = false;
    for (int i = 0; i < item->childCount(); ++i) {
        if (filterItem(item->child(i), text)) anyVisible = true;
    }
    item->setHidden(!anyVisible);
    item->setExpanded(anyVisible && !text.isEmpty());
    return anyVisible;
}

QTreeWidgetItem* LayerSelector::findItemById(const QString& id) const {
    if (!_tree) return nullptr;

    std::function<QTreeWidgetItem*(QTreeWidgetItem*)> find = [&](QTreeWidgetItem* item) -> QTreeWidgetItem* {
        if (item->data(0, IdRole).toString() == id) return item;
        for (int i = 0; i < item->childCount(); ++i) {
            if (auto found = find(item->child(i))) return found;
        }
        return nullptr;
    };

    for (int i = 0; i < _tree->topLevelItemCount(); ++i) {
        if (auto found = find(_tree->topLevelItem(i))) return found;
    }
    return nullptr;
}

void LayerSelector::onItemClicked(QTreeWidgetItem* item) {
    if (!item) return;

    auto id = item->data(0, IdRole).toString();
    if (id.isEmpty()) return;

    _label->setFullText(item->text(0));
    Q_EMIT layerSelected(id);
    _popup->hide();
}

bool LayerSelector::eventFilter(QObject* watched, QEvent* event) {
    if (watched == _searchEdit && event->type() == QEvent::KeyPress) {
        if (handlePopupTreeKey(
                static_cast<QKeyEvent*>(event), _tree, _popup, [this](QTreeWidgetItem* item) { onItemClicked(item); },
                /*leavesOnly=*/false)) {
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

} // namespace Linea::UI
