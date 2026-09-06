// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Page properties widget
 */

#include "page-properties.h"

#include <QComboBox>
#include <QGridLayout>
#include <QIcon>
#include <QMenu>
#include <QRadioButton>
#include <QSignalBlocker>
#include <algorithm>
#include <cmath>

#include "color-picker.h"
#include "number-edit.h"
#include "ui_page-properties.h"
#include "unit-tracker.h"
#include "util/paper.h"
#include "util/units.h"

using Inkscape::PaperSize;
using Inkscape::Util::Quantity;
using Inkscape::Util::Unit;
using Inkscape::Util::UNIT_TYPE_LINEAR;

namespace Linea::UI {

// ---- paper-size sorting helpers (mirrored from Inkscape GTK version) ----

static std::tuple<int, std::string, std::string> get_sorter(const PaperSize& page) {
    const std::string& abbr = page.unit->abbr;
    const std::string& name = page.name;

    if (abbr == "in" && name.find("US") != name.npos) return {0, "US", abbr};

    if (abbr == "mm" && name.size() >= 2 && name[0] >= 'A' && name[0] <= 'E' && name[1] >= '0' && name[1] <= '9') {
        return {1, std::string("ISO ") + name[0], abbr};
    }

    return {2, "Others", abbr};
}

// ---- PageProperties ----

PageProperties::PageProperties(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::PageProperties>())
    , _pageUnitsTracker(std::make_unique<UnitTracker>(UNIT_TYPE_LINEAR, this))
    , _displayUnitsTracker(std::make_unique<UnitTracker>(UNIT_TYPE_LINEAR, this)) {
    _ui->setupUi(this);

    // Configure NumberEdits
    constexpr double MAX_DIM = 100000.0;
    for (auto edit : {_ui->pageWidth, _ui->pageHeight}) {
        edit->setRange(0.001, MAX_DIM);
        edit->setDecimals(4);
        edit->setSingleStep(1.0);
    }
    _ui->scaleX->setRange(0.0001, 10000.0);
    _ui->scaleX->setDecimals(10);
    _ui->scaleX->setSingleStep(0.01);

    for (auto edit : {_ui->viewboxX, _ui->viewboxY}) {
        edit->setRange(-MAX_DIM, MAX_DIM);
        edit->setDecimals(4);
        edit->setSingleStep(1.0);
    }
    for (auto edit : {_ui->viewboxWidth, _ui->viewboxHeight}) {
        edit->setRange(0.001, MAX_DIM);
        edit->setDecimals(4);
        edit->setSingleStep(1.0);
    }

    // Inject tracker-managed combo boxes, replacing the placeholder QComboBox
    // widgets from the .ui file with the ones the tracker owns and keeps in sync.
    auto injectCombo = [](QComboBox* placeholder, QComboBox* replacement) {
        auto lay = qobject_cast<QGridLayout*>(placeholder->parentWidget()->layout());
        if (!lay) return;
        int row = -1, col = -1, rowSpan = 1, colSpan = 1;
        lay->getItemPosition(lay->indexOf(placeholder), &row, &col, &rowSpan, &colSpan);
        lay->removeWidget(placeholder);
        placeholder->hide();
        lay->addWidget(replacement, row, col, rowSpan, colSpan);
    };

    injectCombo(_ui->pageUnitsCombo, _pageUnitsTracker->createUnitCombo(_ui->leftGrid));
    injectCombo(_ui->displayUnitsCombo, _displayUnitsTracker->createUnitCombo(_ui->rightGrid));

    _currentPageUnit = _pageUnitsTracker->getActiveUnit();

    // Portrait/landscape are mutually exclusive
    _ui->portraitButton->setChecked(true);
    _ui->landscapeButton->setChecked(false);

    // Y axis — default down
    _ui->yAxisDownButton->setChecked(true);
    _ui->yAxisUpButton->setChecked(false);

    // _ui->yAxisLayout->setVerticalSpacing(16);

    // Viewbox hidden by default
    setViewboxVisible(false);

    // Scale content lock button: checked = unlocked, unchecked = locked
    _ui->linkScaleContent->setChecked(!_lockedContentScale);
    _ui->linkScaleContent->setIcon(QIcon(_lockedContentScale ? ":/icons/scale-linked" : ":/icons/scale-unlinked"));

    _ui->deskColorButton->setUseTransparency(false);
    _ui->pageColorOpacity->setFactor(100);

    buildTemplateMenu();
    setupConnections();
}

PageProperties::~PageProperties() = default;

QWidget* PageProperties::leftGrid() const {
    return _ui->leftGrid;
}
QWidget* PageProperties::rightGrid() const {
    return _ui->rightGrid;
}

// ---- Template menu ----

void PageProperties::buildTemplateMenu() {
    _pageSizes = PaperSize::getPageSizes();
    std::stable_sort(_pageSizes.begin(), _pageSizes.end(),
                     [](const PaperSize& l, const PaperSize& r) { return get_sorter(l) < get_sorter(r); });

    // Build a QMenu with submenus, attached to templateButton as a popup
    auto menu = new QMenu(this);
    std::string prevSubLabel;
    QMenu* submenu = nullptr;

    for (int i = 0; i < static_cast<int>(_pageSizes.size()); ++i) {
        const auto& page = _pageSizes[i];
        const auto [order, label, abbr] = get_sorter(page);
        if (prevSubLabel != label) {
            submenu = menu->addMenu(QString::fromStdString(label));
            prevSubLabel = label;
        }
        auto action = submenu->addAction(QString::fromStdString(page.getDescription(false)));
        action->setData(i);
        connect(action, &QAction::triggered, this, [this, i]() { onPageTemplateTriggered(i); });
    }

    // Custom sentinel
    auto customAction = menu->addAction(tr("Custom"));
    customAction->setData(static_cast<int>(_pageSizes.size()));
    connect(customAction, &QAction::triggered, this,
            [this, n = static_cast<int>(_pageSizes.size())]() { onPageTemplateTriggered(n); });

    _ui->templateButton->setMenu(menu);
}

// ---- Connections ----

void PageProperties::setupConnections() {
    connect(_ui->pageWidth, SIGNAL(valueChanged(double)), this, SLOT(onPageWidthChanged(double)));
    connect(_ui->pageHeight, SIGNAL(valueChanged(double)), this, SLOT(onPageHeightChanged(double)));

    connect(_ui->viewboxWidth, SIGNAL(valueChanged(double)), this, SLOT(onViewboxWidthChanged(double)));
    connect(_ui->viewboxHeight, SIGNAL(valueChanged(double)), this, SLOT(onViewboxHeightChanged(double)));

    connect(_ui->scaleX, SIGNAL(valueChanged(double)), this, SLOT(onScaleXChanged(double)));

    connect(_ui->portraitButton, &QRadioButton::clicked, this, &PageProperties::onPortraitClicked);
    connect(_ui->landscapeButton, &QRadioButton::clicked, this, &PageProperties::onLandscapeClicked);

    connect(_ui->linkWidthHeight, &QPushButton::clicked, this, &PageProperties::onLinkWidthHeightClicked);

    connect(_ui->linkScaleContent, &QPushButton::toggled, this, &PageProperties::onLinkScaleContentToggled);

    connect(_ui->viewboxToggle, &QPushButton::toggled, this, &PageProperties::onViewboxToggled);

    connect(_ui->pageResizeButton, &QPushButton::clicked, this, &PageProperties::resizeToFit);

    // Decorator checkboxes
    connect(_ui->borderButton, &QCheckBox::toggled, this, [this](bool on) {
        if (!_update.pending()) Q_EMIT checkToggled(on, Check::Border);
    });
    connect(_ui->shadowButton, &QCheckBox::toggled, this, [this](bool on) {
        if (!_update.pending()) Q_EMIT checkToggled(on, Check::Shadow);
    });
    connect(_ui->pageLabelStyleButton, &QCheckBox::toggled, this, [this](bool on) {
        if (!_update.pending()) Q_EMIT checkToggled(on, Check::PageLabelStyle);
    });

    // Checkboxes
    connect(_ui->borderOnTopCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (!_update.pending()) Q_EMIT checkToggled(on, Check::BorderOnTop);
    });
    connect(_ui->antialiasCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (!_update.pending()) Q_EMIT checkToggled(on, Check::AntiAlias);
    });
    connect(_ui->clipToPageCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (!_update.pending()) Q_EMIT checkToggled(on, Check::ClipToPage);
    });
    connect(_ui->originPageCheck, &QCheckBox::toggled, this, [this](bool on) {
        if (!_update.pending()) {
            Q_EMIT checkToggled(on, Check::OriginCurrentPage);
            Q_EMIT originChanged(on);
        }
    });

    // Y-axis buttons
    connect(_ui->yAxisUpButton, &QRadioButton::clicked, this, [this]() {
        if (!_update.pending()) {
            QSignalBlocker b(_ui->yAxisDownButton);
            _ui->yAxisDownButton->setChecked(false);
            _ui->yAxisUpButton->setChecked(true);
            Q_EMIT checkToggled(false, Check::YAxisPointsDown);
        }
    });
    connect(_ui->yAxisDownButton, &QRadioButton::clicked, this, [this]() {
        if (!_update.pending()) {
            QSignalBlocker b(_ui->yAxisUpButton);
            _ui->yAxisUpButton->setChecked(false);
            _ui->yAxisDownButton->setChecked(true);
            Q_EMIT checkToggled(true, Check::YAxisPointsDown);
        }
    });

    // Color pickers
    _ui->backgroundColorButton->setTitle(tr("Background"));
    _ui->borderColorButton->setTitle(tr("Border"));
    _ui->deskColorButton->setTitle(tr("Desk"));
    connect(_ui->backgroundColorButton, &ColorPicker::colorChanged, this, [this](const Inkscape::Colors::Color& c) {
        {
            QSignalBlocker blocker(_ui->pageColorOpacity);
            _ui->pageColorOpacity->setValue(c.getOpacity());
        }
        updatePreviewColor(Color::Background, c);
        if (!_update.pending()) Q_EMIT colorChanged(c, Color::Background);
    });
    connect(_ui->pageColorOpacity, &NumberEdit::valueChanged, this, [this](double opacity) {
        if (_update.pending()) return;

        auto color = _ui->backgroundColorButton->getCurrentColor();
        color.setOpacity(opacity);
        _ui->backgroundColorButton->setColor(color);
        updatePreviewColor(Color::Background, color);
        Q_EMIT colorChanged(color, Color::Background);
    });
    connect(_ui->borderColorButton, &ColorPicker::colorChanged, this, [this](const Inkscape::Colors::Color& c) {
        updatePreviewColor(Color::Border, c);
        if (!_update.pending()) Q_EMIT colorChanged(c, Color::Border);
    });
    connect(_ui->deskColorButton, &ColorPicker::colorChanged, this, [this](const Inkscape::Colors::Color& c) {
        updatePreviewColor(Color::Desk, c);
        if (!_update.pending()) Q_EMIT colorChanged(c, Color::Desk);
    });

    // Unit trackers
    connect(_pageUnitsTracker.get(), &UnitTracker::unitChanged, this, [this](const Unit* unit) {
        onPageUnitChanged(-1); // index unused, tracker owns selection
        Q_UNUSED(unit);
    });
    connect(_displayUnitsTracker.get(), &UnitTracker::unitChanged, this, [this](const Unit* unit) {
        if (!_update.pending()) Q_EMIT unitChanged(unit, Units::Display);
    });
}

// ---- Viewbox visibility ----

void PageProperties::setViewboxVisible(bool visible) {
    for (auto w : {static_cast<QWidget*>(_ui->viewboxPosLabel), static_cast<QWidget*>(_ui->viewboxX),
                   static_cast<QWidget*>(_ui->viewboxY), static_cast<QWidget*>(_ui->viewboxSizeLabel),
                   static_cast<QWidget*>(_ui->viewboxWidth), static_cast<QWidget*>(_ui->viewboxHeight)}) {
        w->setVisible(visible);
    }
}

// ---- Template menu handler ----

void PageProperties::onPageTemplateTriggered(int index) {
    if (_update.pending()) return;

    if (index < 0 || index > static_cast<int>(_pageSizes.size())) return;

    if (index == static_cast<int>(_pageSizes.size())) {
        // Custom sentinel — just fire the current size as PageTemplate
        updatePageSize(true);
        return;
    }

    const auto& page = _pageSizes[index];
    double width = page.width;
    double height = page.height;
    // Preserve current landscape/portrait orientation
    if (_ui->landscapeButton->isChecked() != (width > height)) {
        std::swap(width, height);
    }

    {
        auto scoped = _update.block();
        _ui->pageWidth->setValue(width);
        _ui->pageHeight->setValue(height);
        _pageUnitsTracker->setActiveUnitByAbbr(page.unit->abbr.c_str());
        _currentPageUnit = _pageUnitsTracker->getActiveUnit();
        _ui->docUnitsLabel->setText(QString::fromStdString(page.unit->abbr));
        if (width > 0 && height > 0) _sizeRatio = width / height;
    }

    // Update template button label
    _ui->templateButton->setText(QString::fromStdString(page.name.empty() ? "Custom" : page.name));

    updatePageSize(true);
}

// ---- Page unit changed ----

void PageProperties::onPageUnitChanged(int /*index*/) {
    if (_update.pending()) return;

    const auto oldUnit = _currentPageUnit;
    const auto newUnit = _pageUnitsTracker->getActiveUnit();
    if (!oldUnit || !newUnit || oldUnit == newUnit) return;

    _currentPageUnit = newUnit;

    // Convert displayed values from old unit to new unit
    {
        auto scoped = _update.block();
        Quantity w(_ui->pageWidth->value(), oldUnit->abbr.c_str());
        Quantity h(_ui->pageHeight->value(), oldUnit->abbr.c_str());
        _ui->pageWidth->setValue(w.value(newUnit));
        _ui->pageHeight->setValue(h.value(newUnit));
    }

    _ui->docUnitsLabel->setText(QString::fromStdString(newUnit->abbr));
    updatePageSize();
    Q_EMIT unitChanged(newUnit, Units::Document);
}

void PageProperties::onDisplayUnitChanged(int /*index*/) {
    if (_update.pending()) return;
    Q_EMIT unitChanged(_displayUnitsTracker->getActiveUnit(), Units::Display);
}

// ---- Page size changes ----

void PageProperties::changedLinkedValue(bool widthChanging, NumberEdit* wedit, NumberEdit* hedit) {
    if (_sizeRatio <= 0) return;
    auto scoped = _update.block();
    if (widthChanging) {
        hedit->setValue(wedit->value() / _sizeRatio);
    } else {
        wedit->setValue(hedit->value() * _sizeRatio);
    }
}

void PageProperties::setPageSizeLinked(bool widthChanging) {
    if (_update.pending()) return;
    if (_lockedSizeRatio) {
        changedLinkedValue(widthChanging, _ui->pageWidth, _ui->pageHeight);
    }
    updatePageSize();
}

void PageProperties::setViewboxSizeLinked(bool widthChanging) {
    if (_update.pending()) return;
    if (_scaleIsUniform) {
        changedLinkedValue(widthChanging, _ui->viewboxWidth, _ui->viewboxHeight);
    }
    double w = _ui->viewboxWidth->value();
    double h = _ui->viewboxHeight->value();
    Q_EMIT dimensionChanged(w, h, nullptr, Dimension::ViewboxSize);
}

void PageProperties::updatePageSize(bool templateSelected) {
    bool wasPending = _update.pending();
    auto scoped = _update.block();

    const auto unit = _pageUnitsTracker->getActiveUnit();
    double width = _ui->pageWidth->value();
    double height = _ui->pageHeight->value();

    // Portrait/landscape buttons
    if (std::abs(width - height) > 1e-9) {
        QSignalBlocker bp(_ui->portraitButton), bl(_ui->landscapeButton);
        _ui->portraitButton->setChecked(width <= height);
        _ui->landscapeButton->setChecked(width > height);
        _ui->portraitButton->setEnabled(true);
        _ui->landscapeButton->setEnabled(true);
    } else {
        _ui->portraitButton->setEnabled(false);
        _ui->landscapeButton->setEnabled(false);
    }

    if (width > 0 && height > 0) _sizeRatio = width / height;

    // Find matching template and update button label
    const auto templ = PaperSize::findPaperSize(width, height, unit);
    _ui->templateButton->setText(templ && !templ->name.empty() ? QString::fromStdString(templ->name) : tr("Custom"));

    if (!wasPending) {
        Q_EMIT dimensionChanged(width, height, unit, templateSelected ? Dimension::PageTemplate : Dimension::PageSize);
    }
}

void PageProperties::swapWidthHeight() {
    if (_update.pending()) return;
    {
        auto scoped = _update.block();
        double w = _ui->pageWidth->value();
        double h = _ui->pageHeight->value();
        _ui->pageWidth->setValue(h);
        _ui->pageHeight->setValue(w);
    }
    updatePageSize();
}

// ---- Slot implementations ----

void PageProperties::onPageWidthChanged(double) {
    setPageSizeLinked(true);
}
void PageProperties::onPageHeightChanged(double) {
    setPageSizeLinked(false);
}

void PageProperties::onViewboxWidthChanged(double) {
    setViewboxSizeLinked(true);
}
void PageProperties::onViewboxHeightChanged(double) {
    setViewboxSizeLinked(false);
}

void PageProperties::onScaleXChanged(double v) {
    if (_update.pending()) return;
    Q_EMIT dimensionChanged(v, v, nullptr, _lockedContentScale ? Dimension::ScaleContent : Dimension::Scale);
}

void PageProperties::onPortraitClicked() {
    if (_update.pending()) return;
    double w = _ui->pageWidth->value();
    double h = _ui->pageHeight->value();
    if (w > h) swapWidthHeight();
}

void PageProperties::onLandscapeClicked() {
    if (_update.pending()) return;
    double w = _ui->pageWidth->value();
    double h = _ui->pageHeight->value();
    if (w < h) swapWidthHeight();
}

void PageProperties::onLinkWidthHeightClicked() {
    _lockedSizeRatio = !_lockedSizeRatio;
    _ui->linkWidthHeight->setToolTip(_lockedSizeRatio ? tr("Unlock width/height ratio")
                                                      : tr("Lock width/height ratio"));
    _ui->linkWidthHeight->setIcon(
        QIcon(_lockedSizeRatio && _sizeRatio > 0 ? ":/icons/entries-linked" : ":/icons/entries-unlinked"));
}

void PageProperties::onLinkScaleContentToggled(bool checked) {
    _lockedContentScale = !checked;
    _ui->linkScaleContent->setIcon(QIcon(_lockedContentScale ? ":/icons/scale-linked" : ":/icons/scale-unlinked"));
}

void PageProperties::onViewboxToggled(bool expanded) {
    setViewboxVisible(expanded);
}

// ---- Incoming setters ----

void PageProperties::setColor(Color element, const Inkscape::Colors::Color& color) {
    auto scoped = _update.block();
    getColorPicker(element)->setColor(color);
    if (element == Color::Background) {
        QSignalBlocker blocker(_ui->pageColorOpacity);
        _ui->pageColorOpacity->setValue(color.getOpacity());
    }
    updatePreviewColor(element, color);
}

void PageProperties::setCheck(Check element, bool checked) {
    auto scoped = _update.block();
    switch (element) {
        case Check::NonuniformScale:
            _ui->nonuniformScale->setVisible(checked);
            _scaleIsUniform = !checked;
            _ui->scaleX->setEnabled(_scaleIsUniform);
            break;
        case Check::DisabledScale:
            _ui->scaleX->setEnabled(!checked);
            break;
        case Check::UnsupportedSize:
            _ui->unsupportedSize->setVisible(checked);
            break;
        case Check::Border:
            _ui->borderButton->setChecked(checked);
            break;
        case Check::Shadow:
            _ui->shadowButton->setChecked(checked);
            break;
        case Check::PageLabelStyle:
            _ui->pageLabelStyleButton->setChecked(checked);
            break;
        case Check::BorderOnTop:
            _ui->borderOnTopCheck->setChecked(checked);
            break;
        case Check::AntiAlias:
            _ui->antialiasCheck->setChecked(checked);
            break;
        case Check::ClipToPage:
            _ui->clipToPageCheck->setChecked(checked);
            break;
        case Check::OriginCurrentPage:
            _ui->originPageCheck->setChecked(checked);
            break;
        case Check::YAxisPointsDown:
            _ui->yAxisDownButton->setChecked(checked);
            _ui->yAxisUpButton->setChecked(!checked);
            break;
    }
}

void PageProperties::setDimension(Dimension dimension, double x, double y) {
    auto scoped = _update.block();
    switch (dimension) {
        case Dimension::PageSize:
        case Dimension::PageTemplate:
            _ui->pageWidth->setValue(x);
            _ui->pageHeight->setValue(y);
            updatePageSize(dimension == Dimension::PageTemplate);
            break;
        case Dimension::Scale:
        case Dimension::ScaleContent:
            _ui->scaleX->setValue(x);
            break;
        case Dimension::ViewboxPosition:
            _ui->viewboxX->setValue(x);
            _ui->viewboxY->setValue(y);
            break;
        case Dimension::ViewboxSize:
            _ui->viewboxWidth->setValue(x);
            _ui->viewboxHeight->setValue(y);
            break;
    }
}

void PageProperties::setUnit(Units unit, const QString& abbr) {
    auto scoped = _update.block();
    if (unit == Units::Display) {
        _displayUnitsTracker->setActiveUnitByAbbr(abbr.toStdString().c_str());
    } else {
        _ui->docUnitsLabel->setText(abbr);
        _pageUnitsTracker->setActiveUnitByAbbr(abbr.toStdString().c_str());
        _currentPageUnit = _pageUnitsTracker->getActiveUnit();
        updatePageSize();
    }
}

// ---- Preview color forwarding ----

void PageProperties::updatePreviewColor(Color /*element*/, const Inkscape::Colors::Color& /*color*/) {
    // Preview widget is in _ui->previewBox; a Qt equivalent of PageSizePreview
    // would be added here. For now this is a placeholder.
}

ColorPicker* PageProperties::getColorPicker(Color element) {
    switch (element) {
        case Color::Background:
            return _ui->backgroundColorButton;
        case Color::Border:
            return _ui->borderColorButton;
        case Color::Desk:
            return _ui->deskColorButton;
    }
    Q_UNREACHABLE();
}

} // namespace Linea::UI
