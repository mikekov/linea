// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * RightPanel — the right collapsible panel (properties + symbols).
 */

#ifndef LINEA_UI_DESKTOP_RIGHT_PANEL_H
#define LINEA_UI_DESKTOP_RIGHT_PANEL_H

#include <QToolButton>

#include "ui/widget/collapsible-panel.h"

class QStackedWidget;
class QSettings;
class SPDesktop;

namespace Linea::UI {

class DocumentPropertiesPanel;
class SymbolsWidget;
class StockSymbolsSource;
class NumberEdit;
class PanelSwitch;

class RightPanel : public CollapsiblePanel {
    Q_OBJECT

public:
    explicit RightPanel(QWidget* parent = nullptr);
    ~RightPanel() override;

    /// Build the header toolbar. Call after actions are registered.
    void buildHeader();

    DocumentPropertiesPanel* propertiesPanel() const { return _propertiesPanel; }
    SymbolsWidget* symbolsWidget() const { return _symbolsWidget; }
    PanelSwitch* switchWidget() const { return _panelSwitch; }

    /// Zoom controls (accessed by desktop-widget for zoom display updates).
    void updateZoom(double zoom);
    
    // update canvas rotation display
    void updateRotation(double angle);

    /// Update content for a new active desktop (or null to clear).
    void setDesktop(SPDesktop* desktop);
    void clear();

    void saveSettings(QSettings& settings);
    void restoreSettings(QSettings& settings);

Q_SIGNALS:
    /// Emitted when the user types a new zoom value in the zoom edit.
    void zoomChanged(double zoom);
    void rotationChanged(double angle);

private:
    void buildUi();
    void connectSignals();

    DocumentPropertiesPanel* _propertiesPanel = nullptr;
    SymbolsWidget* _symbolsWidget = nullptr;
    StockSymbolsSource* _symbolsSource = nullptr;
    QStackedWidget* _panelStack = nullptr;
    PanelSwitch* _panelSwitch = nullptr;
    QToolButton* _zoomButton = nullptr;
    NumberEdit* _zoomEdit = nullptr;
    QToolButton* _rotationButton = nullptr;
    NumberEdit* _rotationEdit = nullptr;
};

} // namespace Linea::UI

#endif // LINEA_UI_DESKTOP_RIGHT_PANEL_H
