// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * DashSelector — widget for selecting stroke dash patterns.
 *
 * Shows a button with the current dash pattern. Clicking opens a popup
 * with a grid of predefined patterns that behave like menu items.
 */

#ifndef LINEA_UI_DASH_SELECTOR_H
#define LINEA_UI_DASH_SELECTOR_H

#include <QWidget>
#include <vector>
#include <memory>

#include "ui/operation-blocker.h"

QT_BEGIN_NAMESPACE
class QPushButton;
QT_END_NAMESPACE

namespace Linea::UI {

class DrawingArea;
class PopupMenu;
class DashPatternItem;

} // namespace Linea::UI

QT_BEGIN_NAMESPACE
namespace Ui {
class DashSelectorPopup;
}
QT_END_NAMESPACE

namespace Linea::UI {

/**
 * Widget for selecting dash patterns.
 *
 * The widget displays a button showing the current dash pattern.
 * Clicking the button opens a popup menu with a grid of predefined
 * dash patterns. Users can select a pattern or enter a custom one.
 */
class DashSelector : public QWidget {
    Q_OBJECT

public:
    explicit DashSelector(bool compact = false, QWidget* parent = nullptr);
    ~DashSelector() override;

    /// Set the current dash pattern and offset (combined setter)
    void setDashPattern(const std::vector<double>& dash, double offset);

    /// Get the current dash pattern
    const std::vector<double>& getDashPattern() const { return _dashPattern; }

    /// Get the current dash offset
    double getOffset() const { return _offset; }

    /// Set a placeholder text (shown when pattern is mixed/undefined)
    void setPlaceholder(const QString& placeholder);

    /// Parse dash pattern from string
    static std::vector<double> parseDashPattern(const QString& input);

    /// Format dash pattern to string
    static QString formatDashPattern(const std::vector<double>& pattern);

Q_SIGNALS:
    /// Emitted when dash pattern changes
    void dashChanged();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void construct(bool compact);
    void buildPopupMenu();
    void createPatternGrid();
    void updateButtonDisplay();
    void onPatternItemClicked(int index);

    // Load predefined dash patterns from preferences
    void loadDashPatterns();

    // Data
    std::vector<double> _dashPattern;
    double _offset = 0.0;
    QString _placeholder;

    // Predefined patterns loaded from preferences + custom at CUSTOM_POS
    std::vector<std::vector<double>> _dashPatterns;
    static constexpr int CUSTOM_POS = 2;

    // UI from .ui file
    std::unique_ptr<Ui::DashSelectorPopup> _popupUi;

    // Main button
    QPushButton* _dashButton = nullptr;
    DrawingArea* _dashArea = nullptr;

    // Popup
    PopupMenu* _popup = nullptr;
    QWidget* _popupContent = nullptr;
    std::vector<DashPatternItem*> _patternItems;

    bool _compact = false;
    OperationBlocker _update;

    friend class DashPatternItem; // For accessing _dashPatterns when drawing
};

} // namespace Linea::UI

#endif // LINEA_UI_DASH_SELECTOR_H
