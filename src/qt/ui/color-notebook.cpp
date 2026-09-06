// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * A color selector with RGB, CMYK, CMS, HSL, and Wheel pages (Qt version).
 */

#include "color-notebook.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QPushButton>
#include <QSignalBlocker>
#include <QToolButton>

#include "color-entry.h"
#include "color-page.h"
#include "colors/manager.h"
#include "colors/spaces/base.h"
#include "desktop.h"
#include "icon-combobox.h"
#include "inkscape.h"
#include "preferences.h"
#include "ui/tools/dropper-tool.h"

namespace Linea::UI {

static constexpr int XPAD = 2;
static constexpr int YPAD = 1;

ColorNotebook::ColorNotebook(SPDesktop* desktop, std::shared_ptr<Inkscape::Colors::ColorSet> colors, QWidget* parent)
    : QWidget(parent)
    , _colors(std::move(colors)) {
    setObjectName("ColorNotebook");
    initUI();

    if (!desktop) {
        desktop = SP_ACTIVE_DESKTOP;
    }
    if (desktop) {
        _doc_replaced_connection =
            desktop->connectDocumentReplaced(sigc::hide<0>(sigc::mem_fun(*this, &ColorNotebook::setDocument)));
        setDocument(desktop->getDocument());
    }
}

ColorNotebook::~ColorNotebook() {
    _doc_replaced_connection.disconnect();
    setDocument(nullptr);
    delete _current_page;
}

void ColorNotebook::setDocument(SPDocument* document) {
    _document = document;
    // TODO: Watch for new icc spaces here using the profile tracker
}

void ColorNotebook::setLabel(const QString& label) {
    _label->setText(label);
}

void ColorNotebook::setSwitcherVisible(bool visible) {
    _combo->setVisible(visible);
}

void ColorNotebook::setInkWarningVisible(bool visible) {
    _toomuchink->setVisible(visible);
}

void ColorNotebook::setColorManagedVisible(bool visible) {
    _colormanaged->setVisible(visible);
}

void ColorNotebook::setDropperVisible(bool visible) {
    _btn_picker->setVisible(visible);
}

void ColorNotebook::initUI() {
    _main_layout = new QGridLayout(this);
    _main_layout->setContentsMargins(0, 0, 0, 0);
    int row = 0;

    // Button box with label and combo
    _buttonbox = new QWidget();
    auto buttonLayout = new QHBoxLayout(_buttonbox);
    buttonLayout->setContentsMargins(XPAD, YPAD, XPAD, YPAD);
    buttonLayout->setSpacing(4);

    _label = new QLabel();
    _label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    auto labelFont = _label->font();
    labelFont.setBold(true);
    _label->setFont(labelFont);
    buttonLayout->addWidget(_label, 1, Qt::AlignVCenter);

    _combo = new IconComboBox();
    _combo->setFocusPolicy(Qt::NoFocus);
    _combo->setToolTip(tr("Choose style of color selection"));

    _pickers = Inkscape::Colors::Manager::get().spaces(Inkscape::Colors::Space::Traits::Picker);

    // Add all universal (non-document icc profile) color spaces
    for (int i = 0; i < _pickers.size(); i++) {
        auto& space = _pickers[i];
        auto mode_name = QString::fromStdString(space->getName());
        _combo->addRow(QString::fromStdString(space->getIcon()), mode_name, i);

        // Connect combo selection to space switching
        connect(_combo, &IconComboBox::currentChanged, this, [this](int id) {
            if (id >= 0 && id < _pickers.size()) {
                switchToSpace(_pickers[id]);
            }
        });
    }

    buttonLayout->addWidget(_combo, 0, Qt::AlignVCenter);
    _main_layout->addWidget(_buttonbox, row++, 0);

    // Color page container
    _current_page = nullptr;
    _main_layout->setRowStretch(row, 1);
    row++;

    // Restore the last active page from preferences, defaulting to HSL
    auto prefs = Inkscape::Preferences::get();
    std::string page_name = prefs->getString("/colorselector/page", "HSL");
    bool matched = false;
    for (auto& space : _pickers) {
        if (space->getName() == page_name) {
            switchToSpace(space);
            matched = true;
            break;
        }
    }
    if (!matched && !_pickers.empty()) {
        switchToSpace(_pickers[0]);
    }

    // Combo (space switcher) is hidden by default; callers can show it.
    _combo->setVisible(false);

    // Bottom bar with color management icons, picker, and RGB entry
    auto bottomBar = new QWidget();
    auto bottomLayout = new QHBoxLayout(bottomBar);
    bottomLayout->setContentsMargins(XPAD, 8, XPAD, YPAD);
    bottomLayout->setSpacing(4);

    // Color management icons
    _colormanaged = new QLabel();
    _colormanaged->setPixmap(QIcon(":/icons/color-management").pixmap(16, 16));
    _colormanaged->setToolTip(tr("Color Managed"));
    _colormanaged->setEnabled(false);
    bottomLayout->addWidget(_colormanaged);

    _outofgamut = new QLabel();
    _outofgamut->setPixmap(QIcon(":/icons/out-of-gamut-icon").pixmap(16, 16));
    _outofgamut->setToolTip(tr("Out of gamut!"));
    _outofgamut->setEnabled(false);
    bottomLayout->addWidget(_outofgamut);

    _toomuchink = new QLabel();
    _toomuchink->setPixmap(QIcon(":/icons/too-much-ink-icon").pixmap(16, 16));
    _toomuchink->setToolTip(tr("Too much ink!"));
    _toomuchink->setEnabled(false);
    bottomLayout->addWidget(_toomuchink);

    // Color picker button
    _btn_picker = new QPushButton();
    _btn_picker->setIcon(QIcon(":/icons/color-picker"));
    _btn_picker->setToolTip(tr("Pick colors from image"));
    bottomLayout->addWidget(_btn_picker);

    // RGB label and entry
    auto rgbLabel = new QLabel(tr("RGB"));
    bottomLayout->addWidget(rgbLabel);

    _rgba_entry = new ColorEntry(_colors);
    _rgba_entry->setMaximumWidth(100);
    bottomLayout->addWidget(_rgba_entry, 1);

    _main_layout->addWidget(bottomBar, row++, 0);

    // Connect dropper button
    connect(_btn_picker, &QPushButton::clicked, this, [this]() {
        // Set the dropper into a "one click" mode, so it reverts to the previous tool after a click
        if (_onetimepick) {
            _onetimepick.disconnect();
        } else {
            Inkscape::UI::Tools::sp_toggle_dropper(SP_ACTIVE_DESKTOP);
            auto tool = dynamic_cast<Inkscape::UI::Tools::DropperTool*>(SP_ACTIVE_DESKTOP->getTool());
            if (tool) {
                _onetimepick = tool->onetimepick_signal.connect(
                    [this](const Inkscape::Colors::Color& color) { _colors->setAll(color); });
            }
        }
    });
}

void ColorNotebook::switchToSpace(std::shared_ptr<Inkscape::Colors::Space::AnySpace>& space) {
    // Delete current page if exists
    if (_current_page) {
        _main_layout->removeWidget(_current_page);
        delete _current_page;
        _current_page = nullptr;
    }

    // Create new page for the selected space
    _current_space = space;
    _current_page = new ColorPage(space, _colors);
    _main_layout->addWidget(_current_page, 1, 0); // Add at row 1
    _current_page->show();

    // Sync combo selection, blocking currentChanged to avoid recursion
    for (int i = 0; i < _pickers.size(); i++) {
        if (_pickers[i] == space) {
            QSignalBlocker blocker(_combo);
            _combo->setActiveById(i);
            break;
        }
    }

    // Remember the page selection in preferences
    auto prefs = Inkscape::Preferences::get();
    prefs->setString("/colorselector/page", space->getName());
}

void ColorNotebook::setCurrentColor(std::shared_ptr<Inkscape::Colors::ColorSet>& colors) {
    if (_current_page) {
        // ColorPage doesn't have setCurrentColor, we need to update the color set directly
        _colors = colors;
    }
}

} // namespace Linea::UI
