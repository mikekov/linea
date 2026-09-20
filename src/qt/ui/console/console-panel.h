// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * Resizable panel hosting a ConsoleWidget
 */

#ifndef LINEA_UI_CONSOLE_CONSOLE_PANEL_H
#define LINEA_UI_CONSOLE_CONSOLE_PANEL_H

#include <QEvent>
#include <QString>
#include <memory>

#include "ui/widget/resizable-edge-widget.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class ConsolePanel;
}
QT_END_NAMESPACE

namespace Linea::UI {

class ConsoleWidget;

class ConsolePanel : public ResizableEdgeWidget {
    Q_OBJECT

public:
    explicit ConsolePanel(QWidget* parent = nullptr);
    ~ConsolePanel() override;

    ConsoleWidget* console() const;

    // cap the panel height at the given number of console lines; <= 0 removes the cap
    void setMaxLines(int lines);
    int maxLines() const { return _maxLines; }
    // narrowest console width, in character columns; <= 0 removes the floor
    void setMinColumns(int columns) { _minColumns = qMax(0, columns); }
    int minColumns() const { return _minColumns; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    // snap the console viewport to whole rows/columns; the surrounding
    // chrome (layout margins, header) is excluded from the grid
    QSize snapResize(QSize newSize) const override;

Q_SIGNALS:
    void commandEntered(const QString& command);
    void closeRequested();

private:
    // set the resize step to the console font's character width and line height
    void updateResizeStep();
    // recompute the maximum height from maxLines() and the console font
    void updateMaxHeight();
    // fixed extra space around the console: layout margins + visible header
    QSize chromeSize() const;
    // console area for a panel size: chrome removed, clamped to >= 1 row
    QSize innerSize(QSize panelSize) const;

    std::unique_ptr<Ui::ConsolePanel> _ui;
    int _maxLines = 0;
    int _minColumns = 0;
};

} // namespace Linea::UI

#endif // LINEA_UI_CONSOLE_CONSOLE_PANEL_H
