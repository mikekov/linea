// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * ColorEntry — entry widget for typing a color value in hex CSS form (Qt port).
 */

#include "color-entry.h"

#include "colors/color.h"
#include "colors/spaces/base.h"
#include "colors/spaces/gamut.h"

namespace Linea::UI {

using namespace Inkscape::Colors;

ColorEntry::ColorEntry(std::shared_ptr<ColorSet> colors, QWidget* parent)
    : QLineEdit(parent)
    , _colors(std::move(colors)) {
    setObjectName("ColorEntry");
    setAlignment(Qt::AlignCenter);
    setToolTip(tr("Hexadecimal RGB value of the color"));

    _color_changed_connection = _colors->signal_changed.connect([this]() { onColorChanged(); });

    connect(this, &QLineEdit::textEdited, [this](const QString& text) {
        if (_update.pending()) return;

        auto t = text.trimmed();
        if (looksLikeHex(t)) t = "#" + t;

        auto new_color = Color::parse(t.toStdString());
        if (!new_color) return;

        auto scoped = _update.block();
        if (auto color = _colors->get()) {
            new_color->setOpacity(color->getOpacity());
        }
        _colors->setAll(*new_color);
    });

    onColorChanged();
}

ColorEntry::~ColorEntry() = default;

QSize ColorEntry::sizeHint() const {
    // QLineEdit sizes to maxLength (9 chars) which is unnecessarily wide.
    // Return a compact hint; Expanding policy lets it grow when space is available.
    auto hint = QLineEdit::sizeHint();
    hint.setWidth(fontMetrics().horizontalAdvance('#') * 7 + 8);
    return hint;
}

void ColorEntry::onColorChanged() {
    if (_update.pending()) return;

    if (_colors->isEmpty()) {
        auto scoped = _update.block();
        setText(tr("N/A"));
        return;
    }

    auto color = *_colors->getAverage().converted(Space::Type::RGB);
    if (out_of_gamut(color, color.getSpace())) {
        auto r = color[0], g = color[1], b = color[2];
        auto msg = tr("Color rgb(%1% %2% %3%) is out of sRGB gamut.\nIt has been mapped to sRGB gamut.")
                       .arg(100 * r, 0, 'f', 2)
                       .arg(100 * g, 0, 'f', 2)
                       .arg(100 * b, 0, 'f', 2);
        _signal_out_of_gamut.emit(msg.toStdString());
        _warning = true;
        color = to_gamut_css(color, color.getSpace());
    } else if (_warning) {
        _warning = false;
        _signal_out_of_gamut.emit({});
    }

    auto text = QString::fromStdString(color.toString(false));
    if (this->text() != text) {
        auto scoped = _update.block();
        setText(text);
    }
}

// Returns true for plain hex strings like "fff" or "ff00ff" (no leading '#')
bool ColorEntry::looksLikeHex(const QString& text) const {
    if (text.isEmpty() || text[0] == '#') return false;
    auto len = text.size();
    if (len != 3 && len != 4 && len != 6 && len != 8) return false;
    return std::all_of(text.begin(), text.end(),
                       [](QChar c) { return c.isDigit() || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); });
}

} // namespace Linea::UI
