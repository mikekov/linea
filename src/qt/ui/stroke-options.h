// SPDX-License-Identifier: GPL-2.0-or-later
//
// Stroke options widget — style, pattern, offset, join, cap, miter limit, markers, paint order.

#ifndef LINEA_UI_STROKE_OPTIONS_H
#define LINEA_UI_STROKE_OPTIONS_H

#include <memory>
#include <vector>
#include <QWidget>

#include "ui/operation-blocker.h"

class SPDesktop;
class SPDocument;
class SPStyle;
namespace Linea { struct PresentationState; }

namespace Linea::Props {
class Binder;
class Editor;
}

QT_BEGIN_NAMESPACE
namespace Ui {
class StrokeOptions;
}
QT_END_NAMESPACE

namespace Linea::UI {

class MarkerComboBox;
class PaintOrderWidget;
class PopupMenu;
class RadioToggle;

class StrokeOptions : public QWidget {
    Q_OBJECT

public:
    explicit StrokeOptions(QWidget* parent = nullptr);
    ~StrokeOptions() override;

    void setDocument(SPDocument* document);
    void setDesktop(SPDesktop* desktop);

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
    void setupMarkers();
    void setupPaintOrder();
    void connectSignals();
    void setMiterEnabled(bool enabled);
    void updatePatternEntry(const std::vector<double>& pattern);
    void applyDash(Props::Editor* editor, const std::vector<double>& pattern, double offset);

    std::unique_ptr<Ui::StrokeOptions> _ui;
    RadioToggle* _joinToggle = nullptr;
    RadioToggle* _capToggle = nullptr;
    PaintOrderWidget* _paintOrder = nullptr;
    PopupMenu* _orderPopup = nullptr;
    MarkerComboBox* _markerStart = nullptr;
    MarkerComboBox* _markerMid = nullptr;
    MarkerComboBox* _markerEnd = nullptr;
    SPDesktop* _desktop = nullptr;
    SPDocument* _document = nullptr;
    OperationBlocker _update;
};

} // namespace Linea::UI

#endif // LINEA_UI_STROKE_OPTIONS_H
