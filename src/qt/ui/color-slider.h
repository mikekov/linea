// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * A slider with colored background.
 */

#ifndef LINEA_UI_COLOR_SLIDER_H
#define LINEA_UI_COLOR_SLIDER_H

#include <memory>
#include <vector>
#include <QWidget>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <sigc++/scoped_connection.h>

#include "colors/spaces/components.h"

class QTimer;

namespace Inkscape::Colors {
class ColorSet;
} // namespace Inkscape::Colors

namespace Linea::UI {

/**
 * A slider with a color-gradient track and an animated circular thumb.
 */
class ColorSlider : public QWidget {
    Q_OBJECT
public:
    ColorSlider(std::shared_ptr<Inkscape::Colors::ColorSet> colors,
                Inkscape::Colors::Space::Component component,
                QWidget* parent = nullptr);
    ~ColorSlider() override;

    double getScaled() const;
    void   setScaled(double value);

    const Inkscape::Colors::Space::Component& getComponent() const { return _component; }

Q_SIGNALS:
    void valueChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    QSize sizeHint() const override;

private:
    void updateComponent(double x);
    double valueAtX(double x) const;

    // Tick-style animation driven by a QTimer
    void onAnimationTick();
    void startAnimation();

    std::shared_ptr<Inkscape::Colors::ColorSet> _colors;
    Inkscape::Colors::Space::Component _component;

    // Gradient pixel cache
    std::vector<uint32_t> _gr_buffer;
    int _gr_width = 0;          // width for which _gr_buffer was last built

    bool _dragging = false;
    bool _hover    = false;

    // Animated thumb ring
    double _ring_size      = 0.0;
    double _ring_thickness = 0.0;

    QTimer* _anim_timer = nullptr;
    QElapsedTimer _elapsed;
    qint64 _last_tick_ms = 0;

    sigc::scoped_connection _changed_connection;
};

} // namespace Linea::UI

#endif // LINEA_UI_COLOR_SLIDER_H
