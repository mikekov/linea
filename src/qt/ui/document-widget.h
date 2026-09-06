// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * DocumentWidget — page size, viewbox, coordinate system, and scale settings.
 *
 * Shows PageProperties::leftGrid() and drives the document-side controls.
 * The owner calls setDocument() / setDesktop() when the active document
 * changes, then update() whenever SPNamedView / SPRoot are modified.
 */

#ifndef LINEA_UI_DOCUMENT_WIDGET_H
#define LINEA_UI_DOCUMENT_WIDGET_H

#include <QWidget>
#include "page-properties.h"


class SPDocument;
class SPDesktop;
class SPNamedView;
class SPRoot;

namespace Linea::UI {

/**
 * Document properties widget — page size, viewbox, coordinate system and scale.
 */
class DocumentWidget : public QWidget {
    Q_OBJECT

public:
    explicit DocumentWidget(QWidget* parent = nullptr);
    ~DocumentWidget() override = default;

    void setDocument(SPDocument* document);
    void setDesktop(SPDesktop* desktop);

    /// Call whenever the named view or SVG root changes.
    void update(SPNamedView* namedview, SPRoot* root);

private:
    void onCheckToggled(bool checked, PageProperties::Check element);
    void onDimensionChanged(double x, double y, const Inkscape::Util::Unit* unit,
                            PageProperties::Dimension element);
    void onResizeToFit();

    void updateViewboxUi(SPDocument* document);
    void updateScaleUi(SPDocument* document);

    PageProperties*           _page     = nullptr;
    SPDocument*               _document = nullptr;
    SPDesktop*                _desktop  = nullptr;
    OperationBlocker _update;
};

} // namespace Linea::UI

#endif // LINEA_UI_DOCUMENT_WIDGET_H
