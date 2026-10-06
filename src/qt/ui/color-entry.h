// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * ColorEntry — entry widget for typing a color value in hex CSS form (Qt port).
 */

#ifndef LINEA_UI_COLOR_ENTRY_H
#define LINEA_UI_COLOR_ENTRY_H

#include <memory>
#include <QLineEdit>
#include <sigc++/scoped_connection.h>
#include <sigc++/signal.h>

#include "ui/operation-blocker.h"

namespace Linea::UI {

class ColorHolder;

class ColorEntry : public QLineEdit {
    Q_OBJECT
public:
    explicit ColorEntry(QWidget* parent = nullptr);
    ~ColorEntry() override;

    void setColorHolder(std::shared_ptr<ColorHolder> colors);

    QSize sizeHint() const override;

    sigc::signal<void(std::string)>& outOfGamutSignal() { return _signal_out_of_gamut; }

private:
    void onColorChanged();
    bool looksLikeHex(const QString& text) const;

    std::shared_ptr<ColorHolder> _colors;
    OperationBlocker _update;
    bool _warning = false;
    sigc::scoped_connection _color_changed_connection;
    sigc::signal<void(std::string)> _signal_out_of_gamut;
};

} // namespace Linea::UI

#endif // LINEA_UI_COLOR_ENTRY_H
