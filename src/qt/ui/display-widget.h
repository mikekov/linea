// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * DisplayWidget — display units, rendering settings, and visual preferences.
 *
 * Shows PageProperties::rightGrid() and drives the display-side controls.
 * The owner calls setDocument() when the active document changes, then
 * update() whenever SPNamedView is modified.
 */

#ifndef LINEA_UI_DISPLAY_WIDGET_H
#define LINEA_UI_DISPLAY_WIDGET_H

#include <QWidget>
#include "page-properties.h"


class SPDocument;
class SPNamedView;

namespace Inkscape::Colors { class Color; }
namespace Inkscape::Util   { class Unit;  }

namespace Linea::UI {

/**
 * Display properties widget — colors, display units and rendering options.
 */
class DisplayWidget : public QWidget {
    Q_OBJECT

public:
    explicit DisplayWidget(QWidget* parent = nullptr);
    ~DisplayWidget() override = default;

    void setDocument(SPDocument* document);

    /// Call whenever the named view changes.
    void update(SPNamedView* namedview);

private:
    void onColorChanged(const Inkscape::Colors::Color& color, PageProperties::Color element);
    void onCheckToggled(bool checked, PageProperties::Check element);
    void onUnitChanged(const Inkscape::Util::Unit* unit, PageProperties::Units element);

    void updateDisplayUnitUi(SPNamedView* nv);

    PageProperties*  _page     = nullptr;
    SPDocument*      _document = nullptr;
    OperationBlocker _update;
};

} // namespace Linea::UI

#endif // LINEA_UI_DISPLAY_WIDGET_H
