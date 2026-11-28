// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * AppearancePanel implementation
 */

#include "appearance-panel.h"

#include <QAction>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QToolButton>
#include <QVBoxLayout>
#include <memory>
#include <utility>

#include "blend-mode-combo.h"
#include "desktop.h"
#include "feature-support.h"
#include "image-widget.h"
#include "label-widget.h"
#include "page-widget.h"
#include "path-widget.h"
#include "props/binder.h"
#include "props/property-def.h"
#include "selection.h"
#include "ellipse-widget.h"
#include "rectangle-widget.h"
#include "separator.h"
#include "star-widget.h"
#include "size-widget.h"
#include "style-enums.h"
#include "style-panel.h"
#include "text-panel.h"
#include "ui/tools/text-tool.h"
#include "util/element-properties.h"
#include "util/element-edit.h"
#include "util/object-modified-tags.h"
#include "util/text-utils.h"
#include "widget-utils.h"
#include "ui_appearance-panel.h"

namespace Linea::UI {

const auto TAG = get_next_object_modified_tag();

AppearancePanel::AppearancePanel(QWidget* parent, int leftPadding, int rightPadding)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::AppearancePanel>())
    , _leftPadding(leftPadding)
    , _rightPadding(rightPadding) {
    setupUi();
}

AppearancePanel::~AppearancePanel() = default;

void AppearancePanel::setupUi() {
    _ui->setupUi(this);
    fixColumnToolButtons(this);
    _layout = _ui->mainGrid;

    // Blend mode menu on blendModeButton ("popup-button" class — the
    // stylesheet hides the menu-indicator arrow for it).
    auto menu = new QMenu(this);
    for (auto& entry : blendModeEntries()) {
        if (entry.groupStart) {
            menu->addSeparator();
        }
        auto action = menu->addAction(tr(entry.label));
        action->setCheckable(true);
        action->setData(static_cast<int>(entry.mode));
    }
    _ui->blendModeButton->setMenu(menu);
    _ui->blendModeButton->setPopupMode(QToolButton::InstantPopup);

    // we have 7 columns in a main grid
    int colspan = 7;
    int col = 0;
    // insert rows at the top of the grid
    int row = 0;
    auto layout = _ui->elementPanelsSection;

    // Create label widget
    _labelWidget = new LabelWidget(this);
    _layout->addWidget(_labelWidget, row++, col, 1, colspan);

    // Create size widget
    _sizeWidget = new SizeWidget(this);
    _layout->addWidget(_sizeWidget, row++, col, 1, colspan);

    // the rest: append rows after appearance panel content
    row = 100;

    _pageWidget = new PageWidget();
    _layout->addWidget(_pageWidget, row++, col, 1, colspan);

    _rectangleWidget = new RectangleWidget();
    layout->addWidget(_rectangleWidget);

    _ellipseWidget = new EllipseWidget();
    layout->addWidget(_ellipseWidget);

    _starWidget = new StarWidget();
    layout->addWidget(_starWidget);

    _textPanel = new TextPanel();
    layout->addWidget(_textPanel);

    _imageWidget = new ImageWidget();
    layout->addWidget(_imageWidget);

    _pathWidget = new PathWidget();
    layout->addWidget(_pathWidget);

    // Create style panel
    _stylePanel = new StylePanel(TAG);
    _layout->addWidget(_stylePanel, row++, 0, 1, 7);

    _filterSeparator = new Separator(this);
    _layout->addWidget(_filterSeparator, row++, 0, 1, 7);
    _filterWidget = new FilterWidget();
    _layout->addWidget(_filterWidget, row++, col, 1, colspan);

    _lpeSeparator = new Separator(this);
    _layout->addWidget(_lpeSeparator, row++, 0, 1, 7);
    _lpeWidget = new LpeWidget();
    _layout->addWidget(_lpeWidget, row++, col, 1, colspan);

    _pathOperations = new PathOperations();
    _layout->addWidget(_pathOperations, row++, col, 1, colspan);

    // The "Appearance" section — opacity row plus all element panels —
    // collapses as one unit. elementPanels is a wrapper: its children keep
    // binder-managed visibility inside it, so expand can't force-show them.
    setupCollapsibleSection(_ui->appearanceExpander,
        {_ui->opacityLabel, _ui->opacity, _ui->blendModeButton, _ui->elementPanels},
        "/appearance-panel/appearance_expanded", true);

    // Apply left/right padding to every direct child's layout content margins.
    // Separators are skipped: they span the full panel width by design.
    for (auto child : findChildren<QWidget*>(Qt::FindDirectChildrenOnly)) {
        if (qobject_cast<Separator*>(child)) continue;
        if (child == _stylePanel) {
            // applyHorizontalPadding(child, 0, _rightPadding);
            continue;
        }
        // applyHorizontalPadding(child, _leftPadding, _rightPadding);
    }

    // Separators above the filter, LPE, and path widgets mirror their visibility,
    // so they disappear when the widget they introduce is hidden by the Binder.
    // syncVisibility(_elementSeparator, _stylePanel);
    syncVisibility(_ui->appearanceSeparator, _sizeWidget);
    // syncVisibilityAny(_styleSeparator, {_pageWidget, _rectangleWidget, _ellipseWidget,
        // _starWidget, _textPanel, _imageWidget, _pathWidget});
    syncVisibility(_filterSeparator, _filterWidget);
    syncVisibility(_lpeSeparator, _lpeWidget);

    // Prevent horizontal stretching
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void AppearancePanel::bind(Props::Binder& binder) {
    using namespace Props::Cond;
    
    binder.visibleWhen(_ui->appearanceExpander, hasSelection);

    // Opacity — SpinScale with percent scaling (model 0..1, UI 0..100).
    // setFactor(100) is already applied in setupOpacity().
    binder.bind(Props::opacity, _ui->opacity);

    // Opacity controls visible only when there's a selection.
    binder.visibleWhen(_ui->opacityLabel, hasSelection);
    binder.visibleWhen(_ui->opacity, hasSelection);

    // Reset-opacity button visible when opacity is not uniformly 100%
    // (covers single-with-different-value and mixed selections).
    // binder.visibleWhen(_ui->resetOpacity, hasSelection && differsFrom(Props::opacity, 1.0));

    // Blend mode — checkable menu on blendModeButton; read via bindField,
    // writes via editor->set() held by track().
    binder.bindField(Props::Field::blend_mode, [this](const Props::SelectionState& s) {
        const auto& mode = s.style.blend_mode;
        const int current = mode.is_single() ? static_cast<int>(mode.value()) : -1;
        for (auto action : _ui->blendModeButton->menu()->actions()) {
            action->setChecked(action->data().toInt() == current);
        }
        _ui->blendModeButton->setIcon(QIcon(
            mode.is_mixed() ? ":/icons/blend-mode-mixed"
            : mode.is_single() && mode.value() == SP_CSS_BLEND_NORMAL
                ? ":/icons/blend-mode-normal" : ":/icons/blend-mode-other"));
    });
    binder.track(connect(_ui->blendModeButton->menu(), &QMenu::triggered, this,
        [editor = binder.editor()](QAction* action) {
            editor->set(Props::blend_mode, static_cast<SPBlendMode>(action->data().toInt()));
        }));

    binder.visibleWhen(_ui->blendModeButton, hasSelection);
    // binder.visibleWhen(_ui->bottomSpacer, hasSelection);

    // Reset-blend button visible when blend mode is not uniformly Normal.
    // binder.visibleWhen(_ui->resetBlendBtn, hasSelection && differsFrom(Props::blend_mode, SP_CSS_BLEND_NORMAL));

}

void AppearancePanel::setDesktop(SPDesktop* desktop, SPDocument* document, Props::Binder* binder) {
    _desktop = desktop;
    _binder = binder;
    _editor = binder ? binder->editor() : nullptr;

    _stylePanel->setDesktop(desktop);
    _stylePanel->setDocument(document);

    _textPanel->setDesktop(desktop);
    _textPanel->setDocument(document);

    if (!_desktop) return;

    bind(*_binder);
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
    _pathWidget->bind(*_binder);

    // path operations need two shapes/paths to work
    _binder->visibleWhen(_pathOperations, [](const Props::SelectionState& s) {
        return path_operations_supported(s);
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
    (void)state;
    (void)subselection_active;  // header text now driven by binder via has_text_subselection
    _textPanel->updateTypographyState(getTextTool());
    _textPanel->setVisible(true);
}

} // namespace Linea::UI
