// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * NumberEdit widget - Qt implementation of an enhanced numeric input.
 *
 * Mirrors QDoubleSpinBox functionality with enhancements:
 * - Optional short label or icon
 * - Trim insignificant trailing zeros
 * - Optional frame
 * - Horizontal arrow layout via stylesheets
 * - Expression evaluator
 * - Defocus on Escape key
 *
 */

#ifndef LINEA_UI_NUMBER_EDIT_H
#define LINEA_UI_NUMBER_EDIT_H

#include <functional>
#include <QWidget>
#include "util/mixed-property.h"

class QDoubleSpinBox;
class QLabel;
class QHBoxLayout;
class QIcon;

namespace Linea::UI {

/**
 * Enhanced numeric input widget with optional label/icon and extended features.
 *
 * Composite widget containing a QLabel (for text/icon indicator) and
 * QDoubleSpinBox. The indicator shows either text or icon, never both.
 */
class NumberEdit : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QString label READ label WRITE setLabel)
    Q_PROPERTY(QString icon READ icon WRITE setIcon)
    Q_PROPERTY(double value READ value WRITE setValue)
    Q_PROPERTY(double minimum READ minimum WRITE setMinimum)
    Q_PROPERTY(double maximum READ maximum WRITE setMaximum)
    Q_PROPERTY(int decimals READ decimals WRITE setDecimals)
    Q_PROPERTY(QString prefix READ prefix WRITE setPrefix)
    Q_PROPERTY(QString suffix READ suffix WRITE setSuffix)
    Q_PROPERTY(double singleStep READ singleStep WRITE setSingleStep)
    Q_PROPERTY(bool trimZeros READ trimZeros WRITE setTrimZeros)
    Q_PROPERTY(bool hasFrame READ hasFrame WRITE setHasFrame)

public:
    // Nested spinbox class (defined in .cpp)
    class SpinBox;
    explicit NumberEdit(QWidget* parent = nullptr);
    ~NumberEdit() override;

    // ----------- Indicator (label/icon) -----------

    QString label() const;
    void setLabel(const QString& text);
    QString icon() const;
    void setIcon(const QString& iconName);
    void setIcon(const QIcon& icon);
    void clearIndicator();

    // ----------- Value formatting -----------

    void setTrimZeros(bool trim);
    bool trimZeros() const { return _trimZeros; }

    // ----------- Frame -----------

    void setHasFrame(bool hasFrame);
    bool hasFrame() const { return _hasFrame; }

    // ----------- Callbacks -----------

    void setDefocusCallback(std::function<void()> callback);
    void setEvaluator(std::function<double(const QString&)> evaluator);

    // ----------- Delegated QDoubleSpinBox API -----------

    double value() const;
    void setValue(double val);
    void setValue(const Linea::mixed_property<QVariant>& val);

    QString prefix() const;
    void setPrefix(const QString& prefix);

    QString suffix() const;
    void setSuffix(const QString& suffix);

    double minimum() const;
    void setMinimum(double min);

    double maximum() const;
    void setMaximum(double max);

    void setRange(double min, double max);

    int decimals() const;
    void setDecimals(int prec);

    double singleStep() const;
    void setSingleStep(double step);

    bool wrapping() const;
    void setWrapping(bool wrapping);

    // ----------- Unit scaling -----------
    // When set to a value other than 1.0, the public API (value, setValue,
    // minimum/maximum/step, valueChanged) operates in domain units while the
    // underlying spinbox stores and displays scaled units. E.g. setFactor(100)
    // makes a 0..1 opacity edit display and edit as 0..100.
    double factor() const { return _factor; }
    void setFactor(double factor);

    // ----------- Mixed mode -----------
    void setMixedMode(bool mixed);
    void setPlaceholder(const QString& placeholder);
    void clearPlaceholder();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

Q_SIGNALS:
    void valueChanged(double value);
    void editingFinished();
    void activate();

private Q_SLOTS:
    void onSpinBoxValueChanged(double v);

private:
    bool evaluateAndSet(const QString& text);
    QString formatValue(double value) const;

    QHBoxLayout* _layout = nullptr;
    QLabel* _indicator = nullptr;
    QDoubleSpinBox* _spinBox = nullptr;

    bool _trimZeros = true;
    bool _hasFrame = true;
    bool _editing = false;
    bool _mixedMode = false;
    double _preEditValue = 0.0;
    double _factor = 1.0;

    std::function<void()> _defocusCallback;
    std::function<double(const QString&)> _evaluator;
};

} // namespace Linea::UI

#endif // LINEA_UI_NUMBER_EDIT_H
