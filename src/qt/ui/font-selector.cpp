// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * FontSelector — widget with line edit and button for font selection.
 */

#include "font-selector.h"

#include "util/font-discovery.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QButtonGroup>
#include <QIcon>

namespace Linea::UI {

FontSelector::FontSelector(QWidget* parent)
    : QWidget(parent)
    , _lineEdit(nullptr)
    , _button(nullptr)
    , _popup(nullptr)
    , _popupContent(nullptr)
    , _fontList(nullptr)
    , _searchBox(nullptr)
    , _sortComboBox(nullptr)
    , _optionsButton(nullptr) {

    setAttribute(Qt::WA_StyledBackground, true);
    setupUI();
    setupPopup();
}

FontSelector::~FontSelector() = default;

void FontSelector::setupUI() {
    auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(1);

    _lineEdit = new QLineEdit(this);
    _lineEdit->setPlaceholderText("Select font...");
    _lineEdit->setFrame(false);

    _button = new QPushButton(this);
    _button->setObjectName("SelectorButton");
    _button->setIcon(QIcon(":/icons/pan-start"));

    layout->addWidget(_lineEdit, 1);
    layout->addWidget(_button);

    connect(_button, &QPushButton::clicked, this, &FontSelector::showPopup);
}

void FontSelector::setupPopup() {
    _popup = new PopupMenu(this);

    _popupContent = new QWidget();
    auto popupLayout = new QVBoxLayout(_popupContent);
    popupLayout->setContentsMargins(0, 0, 0, 0);
    popupLayout->setSpacing(8);

    // Top control strip
    auto controlStrip = new QWidget();
    auto controlLayout = new QHBoxLayout(controlStrip);
    controlLayout->setContentsMargins(0, 0, 0, 0);
    controlLayout->setSpacing(4);

    // Sorting combo box
    _sortComboBox = new IconComboBox(controlStrip);
    _sortComboBox->setToolTip(tr("Sort fonts"));
    _sortComboBox->setHeaderType(IconComboBox::ImageOnly);
    _sortComboBox->addRow("sort-by-family", "Sort by Family", static_cast<int>(Inkscape::FontOrder::ByFamily));
    _sortComboBox->addRow("sort-by-weight", "Sort by Weight", static_cast<int>(Inkscape::FontOrder::ByWeight));
    _sortComboBox->addRow("sort-by-width", "Sort by Width", static_cast<int>(Inkscape::FontOrder::ByWidth));
    _sortComboBox->setActiveById(static_cast<int>(Inkscape::FontOrder::ByFamily));

    // Search box
    _searchBox = new QLineEdit(controlStrip);
    _searchBox->setPlaceholderText("Search...");
    _searchBox->setMaximumWidth(150);

    // Options button
    _optionsButton = new QPushButton(controlStrip);
    _optionsButton->setIcon(QIcon(":/icons/gear"));

    controlLayout->addWidget(_sortComboBox);
    controlLayout->addStretch();
    controlLayout->addWidget(_searchBox);
    controlLayout->addWidget(_optionsButton);

    // Font list
    _fontList = new FontList();
    _fontList->setHasFrame(true);
    _fontList->setMinimumSize(300, 800);
    _fontList->refresh(); // Load fonts

    popupLayout->addWidget(controlStrip);
    popupLayout->addWidget(_fontList);

    _popup->setContent(_popupContent);

    // Connect font selection
    connect(_fontList, &FontList::fontSelected, this, [this](const QString& fontspec) {
        _lineEdit->setText(fontspec);
        Q_EMIT fontSelected(fontspec);
        _popup->hide();
    });

    // Connect sorting
    connect(_sortComboBox, &IconComboBox::currentChanged, this, [this](int id) {
        _fontList->setFontOrder(static_cast<Inkscape::FontOrder>(id));
    });

    // Connect search
    connect(_searchBox, &QLineEdit::textChanged, this, [this](const QString& text) {
        _fontList->filterFonts(text);
    });
}

void FontSelector::showPopup() {
    _popup->showLeftOfWidget(_button);
    _fontList->setFocus();
}

} // namespace Linea::UI
