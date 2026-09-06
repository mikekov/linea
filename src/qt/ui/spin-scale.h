// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * SpinScale — Scale and spin button combo widget (Qt version).
 */

#ifndef LINEA_UI_SPIN_SCALE_H
#define LINEA_UI_SPIN_SCALE_H

#include <QWidget>
#include "util/mixed-property.h"

QT_BEGIN_NAMESPACE
class QHBoxLayout;
QT_END_NAMESPACE

namespace Linea::UI {

class NumberEdit;
class ScaleBar;

/**
 * Widget combining a scale bar with a number edit spin button.
 *
 * The scale bar and number edit share the same range and value,
 * providing both visual and precise numeric input.
 */
class SpinScale : public QWidget {
    Q_OBJECT
    Q_PROPERTY(double factor READ factor WRITE setFactor)
    Q_PROPERTY(double minimum READ minimum WRITE setMinimum)
    Q_PROPERTY(double maximum READ maximum WRITE setMaximum)
    Q_PROPERTY(QString suffix READ suffix WRITE setSuffix)
    Q_PROPERTY(int decimals READ decimals WRITE setDecimals)
    Q_PROPERTY(double singleStep READ singleStep WRITE setSingleStep)
    Q_PROPERTY(int maxBlockCount READ maxBlockCount WRITE setMaxBlockCount)

public:
    explicit SpinScale(QWidget* parent = nullptr);
    ~SpinScale() override = default;

    // Value accessors
    double value() const;
    void setValue(double value);
    void setValue(const Linea::mixed_property<QVariant>& val);

    // Range configuration
    void setRange(double min, double max);
    double minimum() const;
    void setMinimum(double min);
    double maximum() const;
    void setMaximum(double max);

    // Formatting
    QString suffix() const;
    void setSuffix(const QString& suffix);
    void setPrefix(const QString& prefix);
    int decimals() const;
    void setDecimals(int digits);
    double singleStep() const;
    void setSingleStep(double step);
    double factor() const;
    void setFactor(double factor);

    // Scale bar configuration
    int maxBlockCount() const { return _maxBlockCount; }
    void setMaxBlockCount(int count);
    void setBlockHeight(int height);

    // Number edit access
    NumberEdit* numberEdit() const { return _numberEdit; }
    ScaleBar* scaleBar() const { return _scaleBar; }

    void setMixedMode(bool mixed);

    QSize sizeHint() const override;

Q_SIGNALS:
    void valueChanged(double value);

private:
    void construct();
    void syncValueToScale(double value);
    void syncValueToSpin(double value);

    NumberEdit* _numberEdit = nullptr;
    ScaleBar* _scaleBar = nullptr;
    QHBoxLayout* _layout = nullptr;
    int _maxBlockCount = 0;
};

} // namespace Linea::UI

#endif // LINEA_UI_SPIN_SCALE_H
