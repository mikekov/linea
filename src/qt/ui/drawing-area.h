// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * DrawingArea — reusable widget for custom drawing with callback
 */

#ifndef LINEA_UI_DRAWING_AREA_H
#define LINEA_UI_DRAWING_AREA_H

#include <QWidget>
#include <QPaintEvent>
#include <functional>

namespace Linea::UI {

/**
 * Reusable drawing widget that mimics GTK's DrawingArea.
 * Takes a callback function to handle custom drawing.
 */
class DrawingArea : public QWidget {
    Q_OBJECT

public:
    using DrawCallback = std::function<void(QPainter*, const QRect&)>;

    explicit DrawingArea(QWidget* parent = nullptr);
    ~DrawingArea() override;

    void setDrawCallback(DrawCallback callback);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    DrawCallback _drawCallback;
};

} // namespace Linea::UI

#endif // LINEA_UI_DRAWING_AREA_H
