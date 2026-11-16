// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * ColorPickerPanel implementation.
 */

#include "color-picker-panel.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QToolButton>
#include <QSpacerItem>
#include <QVBoxLayout>
#include <glibmm/ustring.h>

#include "ui_color-picker-panel.h"

#include "color-clipboard-menu.h"
#include "color-entry.h"
#include "color-page.h"
#include "color-plate.h"
#include "color-preview.h"
#include "preferences.h"

#include "colors/color.h"
#include "colors/manager.h"
#include "colors/spaces/base.h"
#include "desktop.h"
#include "inkscape.h"
#include "qt/util/surface-format.h"
#include "ui/cursor-utils.h"
#include "ui/tools/dropper-tool.h"
#include "ui/tools/tool-base.h"

namespace Linea::UI {

using namespace Inkscape::Colors;

std::unique_ptr<ColorPickerPanel> ColorPickerPanel::create(
    Space::Type space, ColorPlateType type,
    std::shared_ptr<ColorHolder> color, QWidget* parent)
{
    return std::unique_ptr<ColorPickerPanel>(
        new ColorPickerPanel(space, type, std::move(color), parent));
}

ColorPickerPanel::ColorPickerPanel(
    Space::Type space, ColorPlateType type,
    std::shared_ptr<ColorHolder> color, QWidget* parent)
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

ColorPickerPanel::~ColorPickerPanel() { cancelPick(); }

void ColorPickerPanel::buildBottomBar() {
    // eye dropper button
    // connect(_ui->dropperBtn, &QPushButton::clicked, [this]() { pickColor(); });

    // color swatch and hex entry live in the .ui file; only the color holder
    // wiring needs code
    _ui->swatch->setStyle(ColorPreview::Outlined);
    // sharp right corners: the swatch is butt-joined to the entry frame
    _ui->swatch->setFlatEdges(Qt::RightEdge);
    // _ui->swatch->setFrame(true);
    _ui->swatch->setCheckerboardTileSize(4);
    _ui->swatch->setCursor(Qt::PointingHandCursor);
    // _ui->swatch->setToolTip(tr("Click for copy/paste options"));
    _ui->swatch->setShowPipette(true);
    connect(_ui->swatch, &ColorPreview::clicked, this, [this] {
        pickColor();
    });

    _swatchMenu = createColorClipboardMenu(_colorSet, [this] { return _spaceType; }, this, _ui->hexEdit);
    connect(_ui->optionsBtn, &QToolButton::clicked, this, [this] {
        // popup anchored under the button, aligned to its right edge
        const QPoint br = _ui->optionsBtn->mapToGlobal(_ui->optionsBtn->rect().bottomRight());
        _swatchMenu->popup(br - QPoint(_swatchMenu->sizeHint().width(), 0));
    });

    // out-of-gamut warning badge on the swatch; connect before setColorHolder
    // so an already-out-of-gamut color in the holder isn't missed
    _ui->hexEdit->outOfGamutSignal().connect([this](const std::string& msg) {
        _ui->swatch->setWarning(!msg.empty());
        _ui->swatch->setToolTip(msg.empty() ? tr("") : QString::fromStdString(msg));
    });

    _ui->hexEdit->setColorHolder(_colorSet);

    // color space selector — a push button with a popup menu
    _spacesMenu = std::make_unique<QMenu>(this);
    QAction* current = nullptr;
    for (auto&& meta : Inkscape::Colors::Manager::get().spaces(Space::Traits::Picker)) {
        auto type = meta->getType();
        auto text = QString::fromStdString(meta->getShortName());
        auto icon = QIcon(QString(":/icons/%1").arg(QString::fromStdString(meta->getIcon())));
        auto action = _spacesMenu->addAction(icon, text);
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
    }
}

void ColorPickerPanel::createColorPage(Space::Type type, ColorPlateType plateType) {
    auto space = Inkscape::Colors::Manager::get().find(type);

    auto allChannels = plateType == ColorPlateType::None;
    _page = new ColorPage(space, _colorSet, this, ColorPage::ChannelLayout::Segments, allChannels);
    _page->setSpaceMenu(_spacesMenu.get());
    _ui->hexEdit->setDisplaySpace(type);

    int topMargin = 0;
    int index = 0;
    // color plate widget — owned by the layout once inserted
    if (plateType != ColorPlateType::None) {
        const auto gamut = get_plate_gamut_preference();
        _plate = _page->createColorPlate(type, plateType, gamut);
        topMargin = 3;
    }
    if (_plate) {
        // plates need a little breathing room at the top; pages don't
        _topGap = new QSpacerItem(0, topMargin, QSizePolicy::Minimum, QSizePolicy::Fixed);
        _ui->mainLayout->insertItem(index++, _topGap);
        _ui->mainLayout->insertWidget(index++, _plate); // plate goes above the page
    } else {
        // without a plate the page still wants a few pixels of top margin
        _topGap = new QSpacerItem(0, 4, QSizePolicy::Minimum, QSizePolicy::Fixed);
        _ui->mainLayout->insertItem(index++, _topGap);
    }

    _ui->mainLayout->insertWidget(index, _page);

    updateColor();
}

void ColorPickerPanel::removeWidgets() {
    if (_topGap) {
        _ui->mainLayout->removeItem(_topGap);
        delete _topGap;
        _topGap = nullptr;
    }
    // Remove plate widget from layout first (it's a child of _page, deleted with it)
    if (_plate) {
        _ui->mainLayout->removeWidget(_plate);
        delete _plate; // reparented to panel by insertWidget, must delete explicitly
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

    auto color = _colorSet->getOrDefault();
    _ui->swatch->setRgba32(color.toRGBA());
    if (_plate) _plate->setColor(color);
}

void ColorPickerPanel::switchPage(Space::Type space, ColorPlateType plateType) {
    // each space configures a single plate shape — normalize the request so a
    // stale preference can't ask for a shape the space doesn't have
    if (plateType != ColorPlateType::None) {
        plateType = ColorPlate::plateTypeFor(space);
    }
    removeWidgets();
    createColorPage(space, plateType);
    _spaceType = space;
    _plateType = plateType;
    _ui->spacer->changeSize(0, 0, QSizePolicy::Minimum, plateType != ColorPlateType::None ? QSizePolicy::Minimum : QSizePolicy::Expanding);
}

void ColorPickerPanel::setDesktop(SPDesktop* desktop) {
    _desktop = desktop;
}

void ColorPickerPanel::setColor(const Inkscape::Colors::Color& color) {
    _colorSet->set(color);
}

void ColorPickerPanel::setPickerType(Space::Type type) {
    switchPage(type, _plateType);
    // keep the menu's checkmark on the current space
    if (_spacesMenu) {
        for (auto action : _spacesMenu->actions()) {
            auto actionType = static_cast<Space::Type>(action->data().toInt());
            action->setChecked(actionType == type);
        }
    }
}

void ColorPickerPanel::setPlateType(ColorPlateType plate) {
    if (plate == _plateType) return;

    switchPage(_spaceType, plate);
}

ColorPlateType ColorPickerPanel::getPlateType() const {
    return _plateType;
}

sigc::signal<void(Space::Type)>& ColorPickerPanel::colorSpaceChanged() {
    return _colorSpaceChanged;
}

void ColorPickerPanel::pickColor() {
    // clicking the dropper button again while picking cancels the pick
    if (_colorPicking.connected()) {
        cancelPick();
        return;
    }
    auto desktop = _desktop ? _desktop : SP_ACTIVE_DESKTOP;
    if (!desktop) return;

    // Start a detached "one-click" dropper: the current tool stays active, the desktop routes canvas
    // input to the dropper and discards it after a pick, on cancel, or when the document/desktop goes away.
    auto tool = std::make_unique<Inkscape::UI::Tools::DropperTool>(desktop, true);
    _pickTool = tool.get();
    _colorPicking = tool->onetimepick_signal.connect([this](auto& color) {
        _colorSet->set(color);
        endPick();
    });
    _colorPickingCancel = tool->onetimepick_cancel_signal.connect([this] { endPick(); });
    desktop->setOverlayTool(std::move(tool));

    // pick cursor app-wide while the pick is active; the tool's own canvas
    // cursor is the same fixed dropper icon in pickOnly mode, so nothing is lost
    QApplication::setOverrideCursor(
        Inkscape::load_svg_cursor_qt("dropper-tool.svg", devicePixelRatioF()));
}

void ColorPickerPanel::endPick() {
    // the desktop defers discardOverlayTool until the event is dispatched
    _pickTool = nullptr;
    _colorPicking.disconnect();
    _colorPickingCancel.disconnect();
    QApplication::restoreOverrideCursor();
}

void ColorPickerPanel::cancelPick() {
    // the connection only survives while our dropper is still installed on its desktop
    if (!_colorPicking.connected()) return;
    _pickTool->getDesktop()->discardOverlayTool(_pickTool);
    endPick();
}

bool get_show_plate_preference(const std::string& pref_path_base, bool def) {
    Glib::ustring path(pref_path_base);
    return Inkscape::Preferences::get()->getBool(path + "/show-color-plate", def);
}

void set_show_plate_preference(const std::string& pref_path_base, bool show) {
    Glib::ustring path(pref_path_base);
    Inkscape::Preferences::get()->setBool(path + "/show-color-plate", show);
}

PlateGamut get_plate_gamut_preference() {
    auto prefs = Inkscape::Preferences::get();
    PlateGamut gamut;
    const auto name = prefs->getString("/colorselector/plate/gamut", "p3");
    if (name == "p3") {
        gamut.gamut = Gamut::DisplayP3;
    } else if (name == "rec2020") {
        gamut.gamut = Gamut::Rec2020;
    } // unknown values fall back to sRGB
    gamut.srgb_line = prefs->getBool("/colorselector/plate/srgb-line", true);
    gamut.p3_line = prefs->getBool("/colorselector/plate/p3-line", true);
#ifdef __APPLE__
    // same gate OptGLArea uses (src/ui/widget/optglarea.cpp): a Display P3
    // surface reads raw bytes as P3, so plates must convert their sRGB pixels
    if (displaySurfaceIsP3()) {
        gamut.surface = Gamut::DisplayP3;
    }
#endif
    return gamut;
}

} // namespace Linea::UI
