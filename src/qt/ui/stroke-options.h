// SPDX-License-Identifier: GPL-2.0-or-later
//
// Stroke options widget (Qt version) — join, cap, miter limit, paint order.

#ifndef LINEA_UI_STROKE_OPTIONS_H
#define LINEA_UI_STROKE_OPTIONS_H

#include <memory>
#include <QWidget>

#include "ui/operation-blocker.h"

class SPStyle;
namespace Linea { struct PresentationState; }

namespace Linea::Props { class Binder; }

QT_BEGIN_NAMESPACE
namespace Ui {
class StrokeOptions;
}
QT_END_NAMESPACE

namespace Linea::UI {

class PaintOrderWidget;
class RadioToggle;

class StrokeOptions : public QWidget {
    Q_OBJECT

public:
    explicit StrokeOptions(QWidget* parent = nullptr);
    ~StrokeOptions() override;

    // Update UI to reflect the item's style
    void updateWidgets(SPStyle& style);
    void updateWidgets(const Linea::PresentationState& props);

    // Declarative binding through a Binder (replaces updateWidgets + signals
    // when the host panel uses the property system).
    void bind(Props::Binder& binder);

Q_SIGNALS:
    void joinChanged(const char* value);
    void capChanged(const char* value);
    void orderChanged(const char* value);
    void miterChanged(double value);

private:
    void setupJoinCap();
    void setupPaintOrder();
    void connectSignals();
    void setMiterEnabled(bool enabled);

    std::unique_ptr<Ui::StrokeOptions> _ui;
    RadioToggle* _joinToggle = nullptr;
    RadioToggle* _capToggle = nullptr;
    PaintOrderWidget* _paintOrder = nullptr;
    OperationBlocker _update;
};

} // namespace Linea::UI

#endif // LINEA_UI_STROKE_OPTIONS_H
