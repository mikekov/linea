// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Color picker button with ColorNotebook popup
 */

#include "color-picker.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QTimer>
#include <QVBoxLayout>

#include "color-notebook.h"
#include "color-preview.h"
#include "colors/color-set.h"
#include "colors/spaces/components.h"
#include "desktop.h"
#include "document-undo.h"
#include "inkscape.h"
#include "popup-menu.h"

namespace Linea::UI {

ColorPicker::ColorPicker(QWidget* parent)
    : QPushButton(parent)
    , _colors(std::make_shared<Inkscape::Colors::ColorSet>(nullptr, _use_transparency)) {
    _colors->set(Inkscape::Colors::Color(0x000000FFu));
    init();
}

ColorPicker::ColorPicker(SPDesktop* desktop, QString title, QString tip, const Inkscape::Colors::Color& initial,
                         bool undo, bool use_transparency, QWidget* parent)
    : QPushButton(parent)
    , _desktop(desktop)
    , _title(std::move(title))
    , _undo(undo)
    , _use_transparency(use_transparency)
    , _colors(std::make_shared<Inkscape::Colors::ColorSet>(nullptr, use_transparency)) {

    setToolTip(tip);
    _colors->set(initial);
    init();
}

void ColorPicker::init() {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto initial_rgba = _colors->isEmpty() ? 0x000000FFu : _colors->getAverage().toRGBA();

    // Create color preview
    _preview = new ColorPreview(initial_rgba);
    _preview->setStyle(ColorPreview::Simple);
    _preview->setFrame(true);
    _preview->setBorderRadius(1);
    _preview->setCheckerboardTileSize(4);
    // _preview->setFixedSize(16, 16);
    _preview->setFixedHeight(12);
    _preview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Set preview as button content
    auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(5, 0, 5, 0);
    layout->addWidget(_preview, 0, Qt::AlignVCenter);

    // Connect button click to show popup
    connect(this, &QPushButton::clicked, this, [this]() { open(); });

    // Connect color set signals
    _colors->signal_changed.connect(sigc::mem_fun(*this, &ColorPicker::onSelectedColorChanged));
    _colors->signal_released.connect(sigc::mem_fun(*this, &ColorPicker::onSelectedColorChanged));
}

ColorPicker::~ColorPicker() {
    if (_popup) {
        delete _popup;
    }
}

void ColorPicker::setColor(const Inkscape::Colors::Color& color) {
    auto scoped = _update.block();
    _colors->set(color);
    setPreview(color.toRGBA());
}

void ColorPicker::open() {
    if (!_popup) {
        constructPopup();
    }

    if (_popup) {
        _signal_open.emit();
        _popup->showBelowWidget(this);
    }
}

void ColorPicker::close() {
    if (_popup) {
        _popup->hide();
    }
}

void ColorPicker::setTitle(QString title) {
    _title = title;
    if (_color_selector) {
        _color_selector->setLabel(title);
    }
}

void ColorPicker::setIcon(const QString& icon_name) {
    // Replace preview with icon
    delete _preview;
    _preview = nullptr;

    // For now, just use the icon as the button icon
    QPushButton::setIcon(QIcon(QString(":/icons/%1").arg(icon_name)));
}

void ColorPicker::setDesktop(SPDesktop* desktop) {
    _desktop = desktop;
}

void ColorPicker::setUndo(bool undo) {
    _undo = undo;
}

void ColorPicker::setUseTransparency(bool use_transparency) {
    if (_use_transparency == use_transparency) return;

    _use_transparency = use_transparency;
    auto current = getCurrentColor();
    _colors = std::make_shared<Inkscape::Colors::ColorSet>(nullptr, use_transparency);
    _colors->set(current);
    _colors->signal_changed.connect(sigc::mem_fun(*this, &ColorPicker::onSelectedColorChanged));
    _colors->signal_released.connect(sigc::mem_fun(*this, &ColorPicker::onSelectedColorChanged));
    // Rebuild popup so the color selector uses the new ColorSet
    if (_popup) {
        delete _popup;
        _popup = nullptr;
        _color_selector = nullptr;
    }
}

Inkscape::Colors::Color ColorPicker::getCurrentColor() const {
    if (_colors->isEmpty()) {
        return Inkscape::Colors::Color(0x0);
    }
    return _colors->getAverage();
}

sigc::connection ColorPicker::connectChanged(sigc::slot<void(const Inkscape::Colors::Color&)> slot) {
    return _changed_signal.connect(std::move(slot));
}

sigc::signal<void(void)> ColorPicker::signalOpenPopup() {
    return _signal_open;
}

void ColorPicker::onSelectedColorChanged() {
    if (_update.pending()) return;

    auto color = getCurrentColor();
    setPreview(color.toRGBA());
    onColorChanged(color);
}

void ColorPicker::onColorChanged(const Inkscape::Colors::Color& color) {
    if (_undo && _desktop) {
        DocumentUndo::done(_desktop->getDocument(), RC_("Undo", "Set Color"), "");
    }

    _changed_signal.emit(color);
    Q_EMIT colorChanged(color);
}

void ColorPicker::constructPopup() {
    _popup = new PopupMenu();

    // Create ColorNotebook on first popup open (delayed creation)
    _color_selector = new ColorNotebook(_desktop, _colors);
    _color_selector->setLabel(_title);
    _color_selector->setSwitcherVisible(true);
    _color_selector->setInkWarningVisible(false);
    _color_selector->setColorManagedVisible(false);
    _color_selector->setDropperVisible(false);
    _color_selector->setFixedWidth(260);
    _popup->setContent(_color_selector);
}

void ColorPicker::setPreview(std::uint32_t rgba) {
    _preview->setRgba32(rgba);
}

} // namespace Linea::UI
