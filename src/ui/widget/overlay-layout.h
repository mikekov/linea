// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Overlay layout for positioning panels that overlap a first widget.
 *
 * In "floating" mode, panels are positioned over the first widget.
 * In "docked" mode, panels are at the sides, while first widget uses the remaining space.
 */

#ifndef OVERLAY_LAYOUT_H
#define OVERLAY_LAYOUT_H

#include <QLayout>
#include <QRect>
#include <QSize>
#include <QWidget>

namespace Linea::UI {

class OverlayLayout : public QLayout {
    Q_OBJECT

public:
    enum class Position { Left, Right, Center, Canvas };
    enum class Mode { Floating, Docked };

    struct PanelMargins {
        PanelMargins() {}
        PanelMargins(int l, int r, int t, int b) : left(l), right(r), top(t), bottom(b) {}

        int left = 0;
        int right = 0;
        int top = 0;
        int bottom = 0;
    };

    explicit OverlayLayout(QWidget* parent = nullptr);
    ~OverlayLayout() override;

    void addItem(QLayoutItem* item) override;
    QLayoutItem* itemAt(int index) const override;
    QLayoutItem* takeAt(int index) override;
    int count() const override;

    void setGeometry(const QRect& rect) override;
    QSize sizeHint() const override;
    QSize minimumSize() const override;

    void setPanelPosition(QWidget* panel, Position pos, const QSize& initialSize, bool scaleHeight, const PanelMargins& margins = PanelMargins{});
    void setPanelMargins(QWidget* panel, const PanelMargins& margins, bool scaleHeight = true);

    QRect targetGeometry(QWidget* panel) const;
    void updatePanelGeometry();

    Mode panelMode(QWidget* panel) const;
    void setPanelMode(QWidget* panel, Mode mode);

private:
    struct PanelInfo;
    static QRect calculatePanelGeometry(const PanelInfo& panel, QRect& rect);

    struct PanelInfo {
        QWidget* widget = nullptr;
        Position position = Position::Canvas;
        QSize size;
        PanelMargins margins;
        bool scaleHeight = false;
        Mode mode = Mode::Docked;
    };

    QList<QLayoutItem*> _items;
    QList<PanelInfo> _panels;
};

} // namespace Linea::UI

#endif // OVERLAY_LAYOUT_H
