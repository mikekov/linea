// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * ColorPickerPanel implementation.
 */

#include "color-picker-panel.h"

#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QVBoxLayout>
#include <glibmm/ustring.h>

#include "ui_color-picker-panel.h"

#include "color-entry.h"
#include "color-page.h"
#include "color-preview.h"
#include "color-wheel.h"
#include "preferences.h"

#include "colors/color.h"
#include "colors/manager.h"
#include "colors/spaces/base.h"

namespace Linea::UI {

using namespace Inkscape::Colors;

std::unique_ptr<ColorPickerPanel> ColorPickerPanel::create(
    Space::Type space, PlateType type,
    std::shared_ptr<ColorSet> color, QWidget* parent)
{
    return std::unique_ptr<ColorPickerPanel>(
        new ColorPickerPanel(space, type, std::move(color), parent));
}

ColorPickerPanel::ColorPickerPanel(
    Space::Type space, PlateType type,
    std::shared_ptr<ColorSet> color, QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::ColorPickerPanel>())
    , _spaceType(space)
    , _plateType(type)
    , _colorSet(std::move(color))
{
    _ui->setupUi(this);

    buildBottomBar();
    createColorPage(space, type);

    _colorChanged = _colorSet->signal_changed.connect([this]() { updateColor(); });
}

ColorPickerPanel::~ColorPickerPanel() = default;

void ColorPickerPanel::buildBottomBar() {
    // eye dropper button
    connect(_ui->dropperBtn, &QPushButton::clicked, [this]() { pickColor(); });

    // color swatch + hex edit inside the framed container from the .ui file;
    // both take extra constructor arguments, so uic cannot create them
    auto frameLayout = _ui->frameLayout;

    _swatch = new ColorPreview(0, _ui->colorEntryFrame);
    _swatch->setStyle(ColorPreview::Simple);
    _swatch->setFrame(true);
    _swatch->setCheckerboardTileSize(4);
    _swatch->setFixedSize(16, 16);
    frameLayout->insertWidget(0, _swatch);

    _hexEdit = new ColorEntry(_colorSet, _ui->colorEntryFrame);
    _hexEdit->setProperty("class", "button-bar");
    _hexEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    _hexEdit->setFrame(false);
    frameLayout->insertWidget(1, _hexEdit);

    // out-of-gamut warning icon
    _ui->warningLabel->setPixmap(QIcon(":/icons/warning").pixmap(12, 12));
    _hexEdit->outOfGamutSignal().connect([this](const std::string& msg) {
        _ui->warningLabel->setToolTip(msg.empty() ? tr("Color is out of gamut") : QString::fromStdString(msg));
        _ui->warningLabel->setVisible(!msg.empty());
    });

    // color space selector — a push button with a popup menu
    _spacesMenu = std::make_unique<QMenu>(this);
    QAction* current = nullptr;
    for (auto&& meta : Inkscape::Colors::Manager::get().spaces(Space::Traits::Picker)) {
        auto type = meta->getType();
        auto text = QString::fromStdString(meta->getShortName());
        auto action = _spacesMenu->addAction(text);
        action->setCheckable(true);
        action->setData(static_cast<int>(type));
        if (type == _spaceType) current = action;
        connect(action, &QAction::triggered, [this, type]() {
            if (type == Space::Type::NONE) return;
            setPickerType(type);
            _colorSpaceChanged.emit(type);
            Q_EMIT colorSpaceChangedQt(static_cast<int>(type));
        });
    }
    if (current) {
        current->setChecked(true);
        _ui->spacesButton->setText(current->text());
    }
    _ui->spacesButton->setMenu(_spacesMenu.get());
}

void ColorPickerPanel::createColorPage(Space::Type type, PlateType plateType) {
    auto space = Inkscape::Colors::Manager::get().find(type);

    _page = new ColorPage(space, _colorSet, this);

    // color plate/wheel widget — wheel is owned by ColorPage (parented to it),
    // we just hold the interface pointer and a separate widget pointer for the layout.
    if (plateType == Circle) {
        _plate = _page->createColorWheel(type, true);
    } else if (plateType == Rect) {
        _plate = _page->createColorWheel(type, false);
    }
    if (_plate) {
        _plateWidget = _plate->getWidget();
        _ui->mainLayout->insertWidget(0, _plateWidget); // plate goes above the page
    }

    _ui->mainLayout->insertWidget(_plate ? 1 : 0, _page);

    updateColor();
}

void ColorPickerPanel::removeWidgets() {
    // Remove plate widget from layout first (it's a child of _page, deleted with it)
    if (_plateWidget) {
        _ui->mainLayout->removeWidget(_plateWidget);
        delete _plateWidget; // reparented to panel by insertWidget, must delete explicitly
        _plateWidget = nullptr;
        _plate = nullptr;
    }
    if (_page) {
        _ui->mainLayout->removeWidget(_page);
        delete _page;
        _page = nullptr;
    }
}

void ColorPickerPanel::updateColor() {
    if (!_colorSet || _colorSet->isEmpty()) return;

    auto color = _colorSet->getAverage();
    _swatch->setRgba32(color.toRGBA());
    if (_plate) _plate->setColor(color);
}

void ColorPickerPanel::switchPage(Space::Type space, PlateType plateType) {
    removeWidgets();
    createColorPage(space, plateType);
    _spaceType = space;
    _plateType = plateType;
    _ui->spacer->changeSize(0, 0, QSizePolicy::Minimum, plateType != None ? QSizePolicy::Minimum : QSizePolicy::Expanding);
}

void ColorPickerPanel::setDesktop(SPDesktop* desktop) {
    _desktop = desktop;
}

void ColorPickerPanel::setColor(const Inkscape::Colors::Color& color) {
    _colorSet->setAll(color);
}

void ColorPickerPanel::setPickerType(Space::Type type) {
    switchPage(type, _plateType);
    // sync the spaces button: update its label and the checked menu action
    if (_spacesMenu) {
        for (auto* action : _spacesMenu->actions()) {
            auto actionType = static_cast<Space::Type>(action->data().toInt());
            action->setChecked(actionType == type);
            if (actionType == type) _ui->spacesButton->setText(action->text());
        }
    }
}

void ColorPickerPanel::setPlateType(PlateType plate) {
    if (plate == _plateType) return;

    switchPage(_spaceType, plate);
}

ColorPickerPanel::PlateType ColorPickerPanel::getPlateType() const {
    return _plateType;
}

sigc::signal<void(Space::Type)>& ColorPickerPanel::colorSpaceChanged() {
    return _colorSpaceChanged;
}

void ColorPickerPanel::pickColor() {
//     // TODO: port dropper tool integration
//     // Set the dropper into a "one-click" mode, so it reverts to the previous tool after a click
//     if (_colorPicking.connected()) {
//         _colorPicking.disconnect();
//         return;
//     }
//     auto desktop = _desktop ? _desktop : SP_ACTIVE_DESKTOP;
//     if (!desktop) return;
//     Tools::sp_toggle_dropper(desktop);
//     if (auto tool = dynamic_cast<Tools::DropperTool*>(desktop->getTool())) {
//         _colorPicking = tool->onetimepick_signal.connect([this](auto& color) {
//             _colorSet->setAll(color);
//         });
//     }
}

ColorPickerPanel::PlateType get_plate_type_preference(const char* pref_path_base, ColorPickerPanel::PlateType def_type) {
    Glib::ustring path(pref_path_base);
    return static_cast<ColorPickerPanel::PlateType>(Inkscape::Preferences::get()->getIntLimited(path + "/color-plate", def_type, 0, 2));
}

void set_plate_type_preference(const char* pref_path_base, ColorPickerPanel::PlateType type) {
    Glib::ustring path(pref_path_base);
    Inkscape::Preferences::get()->setInt(path + "/color-plate", type);
}

} // namespace Linea::UI
