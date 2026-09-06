// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * ColorEntry — entry widget for typing a color value in hex CSS form (Qt port).
 */

#ifndef LINEA_UI_COLOR_ENTRY_H
#define LINEA_UI_COLOR_ENTRY_H

#include <memory>
#include <QLineEdit>
#include <sigc++/signal.h>

#include "colors/color-set.h"
#include "ui/operation-blocker.h"

namespace Linea::UI {

class ColorEntry : public QLineEdit {
    Q_OBJECT
public:
    explicit ColorEntry(std::shared_ptr<Inkscape::Colors::ColorSet> colors, QWidget* parent = nullptr);
    ~ColorEntry() override;

    QSize sizeHint() const override;

    sigc::signal<void(std::string)>& outOfGamutSignal() { return _signal_out_of_gamut; }

private:
    void onColorChanged();
    bool looksLikeHex(const QString& text) const;

    std::shared_ptr<Inkscape::Colors::ColorSet> _colors;
    OperationBlocker _update;
    bool _warning = false;
    sigc::scoped_connection _color_changed_connection;
    sigc::signal<void(std::string)> _signal_out_of_gamut;
};

} // namespace Linea::UI

#endif // LINEA_UI_COLOR_ENTRY_H
