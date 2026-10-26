// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * StylePanel — combined fill, stroke, opacity and blend mode editor
 */

#ifndef LINEA_UI_STYLE_PANEL_H
#define LINEA_UI_STYLE_PANEL_H

#include <functional>
#include <memory>
#include <vector>
#include <QWidget>

#include "util/style-utils.h"

class SPCSSAttr;
class SPDesktop;
class SPDocument;
class SPItem;
class SPObject;
class QPushButton;
class QLabel;

QT_BEGIN_NAMESPACE
namespace Ui {
class AppearancePanel;
}
QT_END_NAMESPACE

namespace Inkscape::Util {
class Unit;
}

namespace Linea::Props { class Binder; }

namespace Linea::UI {

class IconComboBox;
class NumberEdit;
class PaintSelector;
class PopupMenu;
class StrokeOptions;
class UnitTracker;

/**
 * Compact panel combining fill, stroke, opacity and blend mode controls.
 *
 * Each fill/stroke row has a paint button (opens PaintSelector popup),
 * an alpha spinner, and add/clear buttons.  Stroke attributes (width,
 * dash, join/cap/paint-order, markers) are shown when stroke is defined.
 */
class StylePanel : public QWidget {
    Q_OBJECT

public:
    explicit StylePanel(unsigned int tag, QWidget* parent = nullptr);
    ~StylePanel() override;

    void setDocument(SPDocument* document);
    void setDesktop(SPDesktop* desktop);

    // Declarative binding through a Binder
    void bind(Props::Binder& binder);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    class PaintButton;

    // ---- paint strip (fill or stroke row) ----
    struct PaintStrip {
        bool isFill = true;
        PaintButton* paintBtn = nullptr;
        NumberEdit* alpha = nullptr;
        QPushButton* addBtn = nullptr;
        QPushButton* clearBtn = nullptr;
        QPushButton* toggleBtn = nullptr;
        QWidget* selectorContainer = nullptr;
        QLabel* emptyLabel = nullptr;
        PaintSelector* selector = nullptr;
        PaintSelector* inlineSelector = nullptr;
        PopupMenu* popup = nullptr;
        std::function<bool()> isExpanded;

        void setPreviewFromPaint(const Linea::mixed_property<Linea::PaintProp>& paint,
                                 const Linea::mixed_property<double>& opacity);
        void setPaintFromProps(const Linea::mixed_property<Linea::PaintProp>& paint,
                               const Linea::mixed_property<double>& opacity,
                               const Linea::mixed_property<SPWindRule>& fillRule);
        void setFlatColor(const Inkscape::Colors::Color& color);

        // Declarative binding: paint read via bindField, PaintSelector/add/clear
        // writes via editor->apply() held by binder.track().
        void bind(Props::Binder& binder);
    };

    void construct();
    void setupPaintStrip(PaintStrip& strip, bool isFill);
    void setupStrokeWidgets();
    void setupOpacity();
    void setupBlendMode();

    // members
    std::unique_ptr<Ui::AppearancePanel> _ui;
    PaintStrip _fill;
    PaintStrip _stroke;

    // stroke attribute widgets
    IconComboBox* _unitCombo = nullptr;
    UnitTracker* _unitTracker = nullptr;
    StrokeOptions* _strokeOptions = nullptr;
    PopupMenu* _strokeOptionsPopup = nullptr;
    std::function<bool()> _isStrokeOptionsExpanded;

    // state
    SPDesktop* _desktop = nullptr;
    SPDocument* _document = nullptr;
    const Inkscape::Util::Unit* _currentUnit = nullptr;
    unsigned int _tag;

    // stroke widget group for show/hide
    std::vector<QWidget*> _strokeWidgets;
};

} // namespace Linea::UI

#endif // LINEA_UI_STYLE_PANEL_H
