// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Collapsible panel widget with animation support.
 */

#ifndef LINEA_UI_WIDGET_COLLAPSIBLE_PANEL_H
#define LINEA_UI_WIDGET_COLLAPSIBLE_PANEL_H

#include <QPropertyAnimation>
#include <memory>

#include "ui/widget/resizable-edge-widget.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class CollapsiblePanel;
}
QT_END_NAMESPACE

namespace Linea::UI {

class CollapsiblePanel : public ResizableEdgeWidget {
    Q_OBJECT
    Q_PROPERTY(int collapsedHeight READ collapsedHeight WRITE setCollapsedHeight)
    Q_PROPERTY(bool rounded READ rounded WRITE setRounded)

public:
    explicit CollapsiblePanel(QWidget* parent = nullptr);
    ~CollapsiblePanel() override;

    void setHeader(QWidget* widget);

    void setContentWidget(QWidget* content);
    QWidget* contentWidget() const;

    void setContentMargins(int left, int top, int right, int bottom);

    void setCollapsed(bool collapsed);
    bool isCollapsed() const;

    void setCollapsedHeight(int height);
    int collapsedHeight() const;

    void setAnimationDuration(int msecs);
    int animationDuration() const;

    void setResizeStep(int step);
    int resizeStep() const;

    void setRounded(bool rounded);
    bool rounded() const;

Q_SIGNALS:
    void collapsedChanged(bool collapsed);

private Q_SLOTS:
    void onToggleClicked();

protected:
    bool canResize() const override;
    int snapResizeWidth(int newWidth) const override;

private:
    void updateToggleButtonIcon();
    void updateHeight();

    std::unique_ptr<Ui::CollapsiblePanel> _ui;
    QPropertyAnimation* _animation = nullptr;
    bool _collapsed = false;
    int _collapsedHeight = 40;
    int _animationDuration = 200;

    int _resizeStep = 10;
    bool _rounded = true;
};

} // namespace Linea::UI

#endif // LINEA_UI_WIDGET_COLLAPSIBLE_PANEL_H
