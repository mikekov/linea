// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * AppearancePanel — widget-based component for appearance properties
 */

#ifndef LINEA_UI_APPEARANCE_PANEL_H
#define LINEA_UI_APPEARANCE_PANEL_H

#include <QWidget>
#include "desktop.h"
#include "filter-widget.h"
#include "lpe-widget.h"
#include "path-operations.h"
#include "selection.h"
#include "ui/operation-blocker.h"
#include "util/text-utils.h"

QT_BEGIN_NAMESPACE
class QGridLayout;
class QLabel;
QT_END_NAMESPACE

class SPDesktop;
class SPDocument;

namespace Linea {

class PresentationState;
class ElementState;
class TypographyState;

namespace Props {
class Binder;
class Editor;
}

namespace UI {

class StylePanel;
class SizeWidget;
class LabelWidget;
class RectangleWidget;
class EllipseWidget;
class StarWidget;
class TextPanel;
class ImageWidget;
class LpeWidget;
class Separator;
class PageWidget;

/**
 * Widget-based component for appearance properties.
 *
 * This panel provides a simplified interface for editing appearance
 * properties of selected objects, focusing on appearance-related attributes.
 */
class AppearancePanel : public QWidget {
    Q_OBJECT

public:
    explicit AppearancePanel(QWidget* parent, int leftPadding, int rightPadding);
    ~AppearancePanel() override;

    // void setDocument(SPDocument* document);
    void setDesktop(SPDesktop* desktop, SPDocument* document, Props::Binder* binder);

    Props::Binder* binder() { return _binder; }

Q_SIGNALS:

private:
    void setupUi();
    Inkscape::UI::Tools::TextTool* getTextTool();
    std::pair<std::vector<SPItem*>, bool> getSubselection(Selection* selection);
    void updateText(const TypographyState& state, bool subselection_active);

    int _leftPadding = 0;
    int _rightPadding = 0;
    QGridLayout* _layout = nullptr;
    QLabel* _label = nullptr;
    LabelWidget* _labelWidget = nullptr;
    PageWidget* _pageWidget = nullptr;
    SizeWidget* _sizeWidget = nullptr;
    StylePanel* _stylePanel = nullptr;
    RectangleWidget* _rectangleWidget = nullptr;
    EllipseWidget* _ellipseWidget = nullptr;
    StarWidget* _starWidget = nullptr;
    TextPanel* _textPanel = nullptr;
    ImageWidget* _imageWidget = nullptr;
    LpeWidget* _lpeWidget = nullptr;
    FilterWidget* _filterWidget = nullptr;
    PathOperations* _pathOperations = nullptr;
    Separator* _filterSeparator = nullptr;
    Separator* _lpeSeparator = nullptr;
    Separator* _textSeparator = nullptr;

    // Property system (per-desktop); widgets are re-bound on desktop change.
    // non-owning observing pointers
    Props::Editor* _editor = nullptr;
    Props::Binder* _binder = nullptr;
    SPDesktop* _desktop = nullptr;
};

} // namespace UI
} // namespace Linea

#endif // LINEA_UI_APPEARANCE_PANEL_H
