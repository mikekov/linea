// SPDX-License-Identifier: GPL-2.0-or-later
//
// Created by Michael Kowalski on 11/1/25.
//

#ifndef LINEA_UI_PAINT_INHERITED_H
#define LINEA_UI_PAINT_INHERITED_H

#include <QWidget>
#include <optional>
#include <memory>
#include <array>

#include "ui/widget/paint-enums.h"
#include "ui/operation-blocker.h"

using Inkscape::UI::Widget::PaintDerivedMode;

QT_BEGIN_NAMESPACE
namespace Ui {
class PaintInherited;
}
QT_END_NAMESPACE

namespace Linea::UI {

class PaintInherited : public QWidget {
    Q_OBJECT

public:
    explicit PaintInherited(QWidget* parent = nullptr);
    ~PaintInherited() override;

    // update UI to reflect 'mode'
    void setMode(std::optional<PaintDerivedMode> maybe_mode);

    // get current UI state
    PaintDerivedMode getMode() const;

Q_SIGNALS:
    // signal fired when the user changes inherited paint mode
    void modeChanged(PaintDerivedMode mode);

private Q_SLOTS:
    void onPaintUnsetToggled(bool checked);
    void onPaintInheritToggled(bool checked);
    void onPaintContextFillToggled(bool checked);
    void onPaintContextStrokeToggled(bool checked);
    void onPaintCurrentColorToggled(bool checked);

private:
    void setupConnections();

    std::unique_ptr<Ui::PaintInherited> _ui;
    OperationBlocker _update;
};

} // namespace Linea::UI

#endif // LINEA_UI_PAINT_INHERITED_H
