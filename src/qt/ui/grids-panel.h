// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * GridsPanel — list of document grids with incremental add/remove (Qt version).
 */

#ifndef LINEA_UI_GRIDS_PANEL_H
#define LINEA_UI_GRIDS_PANEL_H

#include <QWidget>
#include <sigc++/scoped_connection.h>

class SPDocument;
class SPGrid;
class SPNamedView;

QT_BEGIN_NAMESPACE
class QLabel;
class QPushButton;
class QScrollArea;
class QVBoxLayout;
QT_END_NAMESPACE

namespace Linea::UI {

class GridWidget;

/**
 * Passive panel that displays a list of document grids.
 *
 * Grids are added and removed incrementally via update() rather than
 * rebuilt in bulk.  set_namedview() must be called (once) so that
 * newly-created grids can bind to the named view.
 */
class GridsPanel : public QWidget {
    Q_OBJECT

public:
    explicit GridsPanel(QWidget* parent = nullptr);
    ~GridsPanel() override = default;

    void setNamedview(SPNamedView* namedview);
    void update(SPNamedView* namedview);

private:
    void addGrid(SPGrid* grid);
    void updatePlaceholder();
    void scrollToBottom();

    void watchDocument();

    // Number of fixed items at the tail of _listLayout: placeholder, button, stretch.
    static constexpr int FIXED_ITEM_COUNT = 3;
    // Number of grid widgets currently in the layout.
    int gridWidgetCount() const;

    SPNamedView* _namedview = nullptr;
    SPDocument* _document = nullptr;
    QVBoxLayout* _listLayout = nullptr;
    QScrollArea* _scroller = nullptr;
    QPushButton* _newGridButton = nullptr;
    QLabel* _noGridsLabel = nullptr;
    sigc::scoped_connection _resourcesChanged;
};

} // namespace Linea::UI

#endif // LINEA_UI_GRIDS_PANEL_H
