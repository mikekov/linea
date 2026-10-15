// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * A color selector with RGB, CMYK, CMS, HSL, and Wheel pages (Qt version).
 */

#include "color-notebook.h"

#include <QCursor>
#include <QMenu>
#include <QPushButton>
#include <QSignalBlocker>

#include "color-clipboard-menu.h"
#include "color-entry.h"
#include "color-page.h"
#include "color-preview.h"
#include "colors/manager.h"
#include "colors/spaces/base.h"
#include "desktop.h"
#include "icon-combobox.h"
#include "inkscape.h"
#include "preferences.h"
#include "ui/tools/dropper-tool.h"
#include "ui_color-notebook.h"

namespace Linea::UI {

ColorNotebook::ColorNotebook(SPDesktop* desktop, std::shared_ptr<ColorHolder> colors, QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::ColorNotebook>())
    , _colors(std::move(colors)) {
    _ui->setupUi(this);
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
    _ui->label->setText(label);
}

void ColorNotebook::setSwitcherVisible(bool visible) {
    _ui->combo->setVisible(visible);
}

void ColorNotebook::setInkWarningVisible(bool visible) {
    _ui->tooMuchInk->setVisible(visible);
}

void ColorNotebook::setColorManagedVisible(bool visible) {
    _ui->colorManaged->setVisible(visible);
}

void ColorNotebook::setDropperVisible(bool visible) {
    _ui->btnPicker->setVisible(visible);
}

void ColorNotebook::initUI() {
    _pickers = Inkscape::Colors::Manager::get().spaces(Inkscape::Colors::Space::Traits::Picker);

    // Add all universal (non-document icc profile) color spaces
    for (int i = 0; i < _pickers.size(); i++) {
        auto& space = _pickers[i];
        auto mode_name = QString::fromStdString(space->getName());
        _ui->combo->addRow(QString::fromStdString(space->getIcon()), mode_name, i);
    }

    // Connect combo selection to space switching
    connect(_ui->combo, &IconComboBox::currentChanged, this, [this](int id) {
        if (id >= 0 && id < _pickers.size()) {
            switchToSpace(_pickers[id]);
        }
    });

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

    // color swatch: shows the current color, click opens copy/paste options
    _ui->swatch->setStyle(ColorPreview::Simple);
    _ui->swatch->setFrame(true);
    _ui->swatch->setCheckerboardTileSize(4);
    _ui->swatch->setCursor(Qt::PointingHandCursor);
    _ui->swatch->setToolTip(tr("Click for copy/paste options"));
    connect(_ui->swatch, &ColorPreview::clicked, this, [this] {
        _swatchMenu->popup(QCursor::pos());
    });

    wireColorHolder();

    // out-of-gamut warning icon
    _ui->rgbaEntry->outOfGamutSignal().connect([this](const std::string& msg) {
        _ui->outOfGamut->setToolTip(msg.empty() ? tr("Out of gamut!") : QString::fromStdString(msg));
        _ui->outOfGamut->setVisible(!msg.empty());
    });

    // Connect dropper button
    connect(_ui->btnPicker, &QPushButton::clicked, this, [this]() {
        // Set the dropper into a "one click" mode, so it reverts to the previous tool after a click
        if (_onetimepick) {
            _onetimepick.disconnect();
        } else {
            Inkscape::UI::Tools::sp_toggle_dropper(SP_ACTIVE_DESKTOP);
            auto tool = dynamic_cast<Inkscape::UI::Tools::DropperTool*>(SP_ACTIVE_DESKTOP->getTool());
            if (tool) {
                _onetimepick = tool->onetimepick_signal.connect(
                    [this](const Inkscape::Colors::Color& color) { _colors->set(color); });
            }
        }
    });
}

// Bind _colors to the UI: hex entry, swatch display, and the swatch's
// copy/paste menu. Called on construction and again whenever setCurrentColor
// swaps in a different holder.
void ColorNotebook::wireColorHolder() {
    if (!_colors) return;

    _ui->rgbaEntry->setColorHolder(_colors);
    _colorChanged = _colors->signal_changed.connect([this] { updateSwatch(); });
    updateSwatch();

    delete _swatchMenu;
    _swatchMenu = createColorClipboardMenu(_colors, [this] {
        return _current_space ? _current_space->getType() : Inkscape::Colors::Space::Type::NONE;
    }, this);
}

void ColorNotebook::updateSwatch() {
    if (!_colors || _colors->isEmpty()) return;

    _ui->swatch->setRgba32(_colors->getOrDefault().toRGBA());
}

void ColorNotebook::switchToSpace(std::shared_ptr<Inkscape::Colors::Space::AnySpace>& space) {
    // Delete current page if exists
    if (_current_page) {
        _ui->mainLayout->removeWidget(_current_page);
        delete _current_page;
        _current_page = nullptr;
    }

    // Create new page for the selected space
    _current_space = space;
    _current_page = new ColorPage(space, _colors);
    _ui->mainLayout->addWidget(_current_page, 1, 0); // Add at row 1
    _current_page->show();

    // Sync combo selection, blocking currentChanged to avoid recursion
    for (int i = 0; i < _pickers.size(); i++) {
        if (_pickers[i] == space) {
            QSignalBlocker blocker(_ui->combo);
            _ui->combo->setActiveById(i);
            break;
        }
    }

    // Remember the page selection in preferences
    auto prefs = Inkscape::Preferences::get();
    prefs->setString("/colorselector/page", space->getName());
}

void ColorNotebook::setCurrentColor(std::shared_ptr<ColorHolder>& colors) {
    if (colors == _colors) return;

    _colors = colors;
    wireColorHolder();
    // ColorPage binds the holder at construction — rebuild it on the new holder
    if (_current_space) {
        switchToSpace(_current_space);
    }
}

} // namespace Linea::UI
