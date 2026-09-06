// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Page properties widget
 */

#ifndef LINEA_UI_PAGE_PROPERTIES_H
#define LINEA_UI_PAGE_PROPERTIES_H

#include <memory>
#include <vector>
#include <QWidget>
#include "colors/color.h"
#include "ui/operation-blocker.h"
#include "util/paper.h"

namespace Ui { class PageProperties; }
namespace Inkscape::Util { class Unit; }

namespace Linea::UI {

class NumberEdit;
class ColorPicker;
class UnitTracker;

/**
 * Page properties widget.
 *
 * Mirrors Inkscape::UI::Widget::PageProperties but uses Qt6 widgets.
 * The widget is driven externally: the owner calls set_color(),
 * set_check(), set_dimension() and set_unit() when the document changes,
 * and connects the signals below to write changes back.
 */
class PageProperties : public QWidget {
    Q_OBJECT

public:
    explicit PageProperties(QWidget* parent = nullptr);
    ~PageProperties() override;

    // ---- grid accessors — embed only one in any given panel ----
    // leftGrid:  document properties (page size, viewbox, scale, coordinate system)
    // rightGrid: display properties (colors, display units, rendering options)
    QWidget* leftGrid() const;
    QWidget* rightGrid() const;

    // ---- incoming setters (called by the owner when the document changes) ----

    enum class Color { Background, Desk, Border };
    void setColor(Color element, const Inkscape::Colors::Color& color);

    enum class Check {
        Border, Shadow, BorderOnTop, AntiAlias,
        NonuniformScale, DisabledScale, UnsupportedSize,
        ClipToPage, PageLabelStyle, YAxisPointsDown, OriginCurrentPage
    };
    void setCheck(Check element, bool checked);

    enum class Dimension {
        PageSize, ViewboxSize, ViewboxPosition, Scale, ScaleContent, PageTemplate
    };
    void setDimension(Dimension dim, double x, double y);

    enum class Units { Display, Document };
    void setUnit(Units unit, const QString& abbr);

Q_SIGNALS:
    void colorChanged(const Inkscape::Colors::Color& color, PageProperties::Color element);
    void checkToggled(bool checked, PageProperties::Check element);
    // unit pointer is valid only for PageSize / PageTemplate dimensions
    void dimensionChanged(double x, double y, const Inkscape::Util::Unit* unit,
                          PageProperties::Dimension dimension);
    void unitChanged(const Inkscape::Util::Unit* unit, PageProperties::Units which);
    void resizeToFit();
    void originChanged(bool originOnCurrentPage);

private Q_SLOTS:
    void onPageWidthChanged(double v);
    void onPageHeightChanged(double v);
    void onViewboxWidthChanged(double v);
    void onViewboxHeightChanged(double v);
    void onScaleXChanged(double v);
    void onPortraitClicked();
    void onLandscapeClicked();
    void onLinkWidthHeightClicked();
    void onLinkScaleContentToggled(bool checked);
    void onViewboxToggled(bool expanded);
    void onPageTemplateTriggered(int index);
    void onPageUnitChanged(int index);
    void onDisplayUnitChanged(int index);

private:
    void setupConnections();
    void buildTemplateMenu();
    void setViewboxVisible(bool visible);
    void setPageSizeLinked(bool widthChanging);
    void setViewboxSizeLinked(bool widthChanging);
    void changedLinkedValue(bool widthChanging, NumberEdit* wedit, NumberEdit* hedit);
    void updatePageSize(bool templateSelected = false);
    void swapWidthHeight();
    void updatePreviewColor(Color element, const Inkscape::Colors::Color& color);
    ColorPicker* getColorPicker(Color element);

    std::unique_ptr<Ui::PageProperties> _ui;
    std::unique_ptr<UnitTracker> _pageUnitsTracker;
    std::unique_ptr<UnitTracker> _displayUnitsTracker;

    const Inkscape::Util::Unit* _currentPageUnit = nullptr;
    double _sizeRatio = 1.0;
    bool _lockedSizeRatio = false;
    bool _scaleIsUniform = true;
    bool _lockedContentScale = false;
    OperationBlocker _update;
    std::vector<Inkscape::PaperSize> _pageSizes;
};

} // namespace Linea::UI

#endif // LINEA_UI_PAGE_PROPERTIES_H
