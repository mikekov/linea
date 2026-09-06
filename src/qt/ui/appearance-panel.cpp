// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * AppearancePanel implementation
 */

#include "appearance-panel.h"

#include <QLabel>
#include <QVBoxLayout>
#include <memory>
#include <utility>

#include "desktop.h"
#include "image-widget.h"
#include "label-widget.h"
#include "page-widget.h"
#include "props/binder.h"
#include "selection.h"
#include "ellipse-widget.h"
#include "rectangle-widget.h"
#include "separator.h"
#include "star-widget.h"
#include "size-widget.h"
#include "style-panel.h"
#include "text-panel.h"
#include "ui/tools/text-tool.h"
#include "util/element-properties.h"
#include "util/element-edit.h"
#include "util/object-modified-tags.h"
#include "util/text-utils.h"
#include "widget-utils.h"

namespace Linea::UI {

const auto TAG = get_next_object_modified_tag();

AppearancePanel::AppearancePanel(QWidget* parent, int leftPadding, int rightPadding)
    : QWidget(parent)
    , _leftPadding(leftPadding)
    , _rightPadding(rightPadding) {
    setupUi();
}

AppearancePanel::~AppearancePanel() = default;

void AppearancePanel::setupUi() {
    _layout = new QGridLayout(this);
    _layout->setContentsMargins(0, 0, 0, 0);
    _layout->setSpacing(4);

    int row = 0;
    // Create label widget
    _labelWidget = new LabelWidget(this);
    _layout->addWidget(_labelWidget, row++, 0, 1, 3);

    // Create size widget
    _sizeWidget = new SizeWidget(this);
    _layout->addWidget(_sizeWidget, row++, 0, 1, 3);

    _pageWidget = new PageWidget();
    _layout->addWidget(_pageWidget, row++, 0, 1, 3);

    // Create style panel
    _stylePanel = new StylePanel(TAG);
    // _stylePanel->setDelegate(std::make_unique<Util::ElementEdit>(TAG));
    _layout->addWidget(_stylePanel, row++, 0, 1, 3);

    _rectangleWidget = new RectangleWidget();
    _layout->addWidget(_rectangleWidget, row++, 0, 1, 3);

    _ellipseWidget = new EllipseWidget();
    _layout->addWidget(_ellipseWidget, row++, 0, 1, 3);

    _starWidget = new StarWidget();
    _layout->addWidget(_starWidget, row++, 0, 1, 3);

    _textSeparator = new Separator(this);
    _layout->addWidget(_textSeparator, row++, 0, 1, 3);

    _textPanel = new TextPanel();
    _layout->addWidget(_textPanel, row++, 0, 1, 3);

    _imageWidget = new ImageWidget();
    _layout->addWidget(_imageWidget, row++, 0, 1, 3);

    _filterSeparator = new Separator(this);
    _layout->addWidget(_filterSeparator, row++, 0, 1, 3);

    _filterWidget = new FilterWidget();
    _layout->addWidget(_filterWidget, row++, 0, 1, 3);

    _lpeSeparator = new Separator(this);
    _layout->addWidget(_lpeSeparator, row++, 0, 1, 3);

    _lpeWidget = new LpeWidget();
    _layout->addWidget(_lpeWidget, row++, 0, 1, 3);

    _pathOperations = new PathOperations();
    _layout->addWidget(_pathOperations, row++, 0, 1, 3);

    // Apply left/right padding to every direct child's layout content margins.
    // Separators are skipped: they span the full panel width by design.
    for (auto child : findChildren<QWidget*>(Qt::FindDirectChildrenOnly)) {
        if (qobject_cast<Separator*>(child)) continue;
        applyHorizontalPadding(child, _leftPadding, _rightPadding);
    }

    // Separators above the filter and LPE widgets mirror their visibility, so
    // they disappear when the widget they introduce is hidden by the Binder.
    syncVisibility(_textSeparator, _textPanel);
    syncVisibility(_filterSeparator, _filterWidget);
    syncVisibility(_lpeSeparator, _lpeWidget);

    // Prevent horizontal stretching
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void AppearancePanel::setDesktop(SPDesktop* desktop, SPDocument* document, Props::Binder* binder) {
    _desktop = desktop;
    _binder = binder;
    _editor = binder ? binder->editor() : nullptr;

    _stylePanel->setDesktop(desktop);
    _stylePanel->setDocument(document);

    // Set desktop on size widget for transform panel
    _sizeWidget->setDesktop(desktop);
    _textPanel->setDesktop(desktop);
    _textPanel->setDocument(document);

    if (!_desktop) return;

    _labelWidget->bind(*_binder);
    _sizeWidget->bind(*_binder);
    _pageWidget->bind(*_binder);
    _stylePanel->bind(*_binder);
    _rectangleWidget->bind(*_binder);
    _ellipseWidget->bind(*_binder);
    _starWidget->bind(*_binder);
    _imageWidget->bind(*_binder);
    _textPanel->bind(*_binder);
    _filterWidget->bind(*_binder);
    _lpeWidget->bind(*_binder);

    // path operations need two shapes/paths to work
    _binder->visibleWhen(_pathOperations, [](const Props::SelectionState& s) {
        auto& c = s.element.count;
        auto n = c.paths + c.rectangles + c.ellipses + c.stars + c.polygons + c.lines + c.textual;
        return n >= 2;
    });

}

Inkscape::UI::Tools::TextTool* AppearancePanel::getTextTool() {
    if (!_desktop) return nullptr;

    return dynamic_cast<Inkscape::UI::Tools::TextTool*>(_desktop->getTool());
}

std::pair<std::vector<SPItem*>, bool> AppearancePanel::getSubselection(Selection* selection) {
    auto tool = getTextTool();
    if (selection->single() && tool) {
        auto span = tool->get_subselection(true);
        if (!span.empty()) return std::make_pair(span, true);
    }

    return std::make_pair(Linea::get_all_text_spans(selection), false);
}

void AppearancePanel::updateText(const TypographyState& state, bool subselection_active) {
    (void)subselection_active;  // header text now driven by binder via has_text_subselection
    // _textPanel->setVisible(present && el.count.)
    _textPanel->updateTypographyState(state, getTextTool());
    _textPanel->setVisible(true);
}


} // namespace Linea::UI
