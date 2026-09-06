// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * GridsPanel — list of document grids with incremental add/remove (Qt version).
 */

#include "grids-panel.h"

#include <glibmm/i18n.h>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

#include "grid-widget.h"

#include "document.h"
#include "document-undo.h"
#include "object/sp-grid.h"
#include "object/sp-namedview.h"

namespace {

void create_new_grid(SPNamedView* namedview) {
    if (!namedview || !namedview->document) return;

    auto repr = namedview->getRepr();
    SPGrid::create_new(namedview->document, repr, GridType::RECTANGULAR);

    namedview->newGridCreated();

    Inkscape::DocumentUndo::done(namedview->document,
        RC_("Undo", "Create new grid"), "");
}

} // namespace

namespace Linea::UI {

int GridsPanel::gridWidgetCount() const {
    return _listLayout->count() - FIXED_ITEM_COUNT;
}

GridsPanel::GridsPanel(QWidget* parent)
    : QWidget(parent)
{
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(4);

    // Scroll area for the grid list
    _scroller = new QScrollArea(this);
    _scroller->setWidgetResizable(true);
    _scroller->setFrameShape(QFrame::NoFrame);
    _scroller->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto listContainer = new QWidget(_scroller);
    _listLayout = new QVBoxLayout(listContainer);
    _listLayout->setContentsMargins(0, 0, 0, 0);
    _listLayout->setSpacing(0);

    // "No grids" placeholder — shown inside the list when empty.
    _noGridsLabel = new QLabel(QString::fromUtf8(_("No grids defined")), listContainer);
    _noGridsLabel->setAlignment(Qt::AlignCenter);
    _noGridsLabel->setProperty("class", "placeholder-label");
    _noGridsLabel->setMargin(20);
    _listLayout->addWidget(_noGridsLabel);

    // "New Grid" button — sits below the defined grids, inside the scroll area.
    // It stays at a fixed position: after the grids, before the stretch.
    _newGridButton = new QPushButton(QIcon::fromTheme("list-add"),
                                     QString::fromUtf8(_("New Grid")), listContainer);
    _newGridButton->setToolTip(QString::fromUtf8(_("Add a new grid to the document")));
    _listLayout->addWidget(_newGridButton);

    _listLayout->addStretch();
    _scroller->setWidget(listContainer);

    mainLayout->addWidget(_scroller, 1);

    connect(_newGridButton, &QPushButton::clicked, this, [this]() {
        create_new_grid(_namedview);
        scrollToBottom();
    });

    updatePlaceholder();
}

void GridsPanel::setNamedview(SPNamedView* namedview) {
    if (_namedview == namedview) return;

    _namedview = namedview;
    _document = namedview ? namedview->document : nullptr;
    watchDocument();
}

void GridsPanel::watchDocument() {
    _resourcesChanged.disconnect();

    if (!_document) return;

    _resourcesChanged = _document->connectResourcesChanged("grid", [this]() {
        // Defer to the next idle loop iteration so that the widget whose
        // action triggered the resource change finishes its handler cleanly.
        QTimer::singleShot(0, this, [this]() { update(_namedview); });
    });
}

void GridsPanel::update(SPNamedView* namedview) {
    if (!namedview) {
        // Remove all grid widgets (keep placeholder, button, and stretch)
        while (gridWidgetCount() > 0) {
            auto item = _listLayout->takeAt(0);
            delete item->widget();
            delete item;
        }
        updatePlaceholder();
        return;
    }

    const auto& grids = namedview->grids;

    int widgetCount = gridWidgetCount();
    int gridIndex = 0;

    // Sync existing widgets or add new ones
    while (gridIndex < static_cast<int>(grids.size())) {
        if (gridIndex < widgetCount) {
            // Update existing widget if it watches a different grid
            auto* item = _listLayout->itemAt(gridIndex);
            auto* widget = qobject_cast<GridWidget*>(item->widget());
            if (widget && widget->grid() != grids[gridIndex]) {
                widget->setGrid(grids[gridIndex]);
            }
        } else {
            addGrid(grids[gridIndex]);
        }
        gridIndex++;
    }

    // Remove excess widgets (iterate backwards, before the fixed tail)
    widgetCount = gridWidgetCount();
    while (widgetCount > static_cast<int>(grids.size())) {
        auto item = _listLayout->takeAt(widgetCount - 1);
        delete item->widget();
        delete item;
        widgetCount--;
    }

    updatePlaceholder();
}

void GridsPanel::addGrid(SPGrid* grid) {
    if (!grid) return;

    auto widget = new GridWidget(grid, this);
    widget->setContentsMargins(0, 0, 0, 12);

    // Insert at the end of the grid region, before the fixed tail.
    _listLayout->insertWidget(gridWidgetCount(), widget);

    updatePlaceholder();
}

void GridsPanel::updatePlaceholder() {
    _noGridsLabel->setVisible(gridWidgetCount() <= 0);
    _scroller->setVisible(true);
}

void GridsPanel::scrollToBottom() {
    QTimer::singleShot(0, this, [this]() {
        if (auto bar = _scroller->verticalScrollBar()) {
            bar->setValue(bar->maximum());
        }
    });
}

} // namespace Linea::UI
