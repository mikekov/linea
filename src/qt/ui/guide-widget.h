// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * GuideWidget — widget for editing guide properties
 */

#ifndef LINEA_UI_GUIDE_WIDGET_H
#define LINEA_UI_GUIDE_WIDGET_H

#include <QWidget>
#include "colors/color.h"

QT_BEGIN_NAMESPACE
class QLineEdit;
class QPushButton;
namespace Ui {
class GuideWidget;
}
QT_END_NAMESPACE

class SPGuide;

namespace Linea::UI {

class ColorButton;
class NumberEdit;

/**
 * Widget for editing guide properties.
 *
 * Provides controls for:
 * - Label: text label for the guide
 * - Color: color picker for guide color
 * - Position: X and Y coordinates
 * - Angle: rotation angle
 * - Locked: lock/unlock state
 */
class GuideWidget : public QWidget {
    Q_OBJECT

public:
    explicit GuideWidget(QWidget* parent = nullptr);
    ~GuideWidget() override;

    // element ID
    void setGuideId(const QString& id);

    // Label accessors
    QString label() const;
    void setLabel(const QString& label);

    // Color accessors
    Inkscape::Colors::Color color() const;
    void setColor(const Inkscape::Colors::Color& color);

    // Position accessors
    double x() const;
    double y() const;
    void setPosition(double x, double y);
    void setX(double x);
    void setY(double y);

    // Angle accessor
    double angle() const;
    void setAngle(double angle);

    // Locked accessor
    bool isLocked() const;
    void setLocked(bool locked);

    void setGuide(SPGuide* guide) { _guide = guide; }
    SPGuide* guide() const { return _guide; }

Q_SIGNALS:
    void labelChanged(const QString& label);
    void colorChanged(const Inkscape::Colors::Color& color);
    void positionChanged(double x, double y);
    void angleChanged(double angle);
    void lockedChanged(bool locked);
    void deleteGuide();

private Q_SLOTS:
    void onLabelChanged(const QString& label);
    void onColorChanged(const Inkscape::Colors::Color& color);
    void onXChanged(double x);
    void onYChanged(double y);
    void onAngleChanged(double angle);
    void onLockedChanged(bool checked);

private:
    std::unique_ptr<Ui::GuideWidget> _ui;
    SPGuide* _guide = nullptr;
};

} // namespace Linea::UI

#endif // LINEA_UI_GUIDE_WIDGET_H
