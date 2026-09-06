// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * SpinScale — Scale and spin button combo widget (Qt version).
 */

#include "spin-scale.h"

#include <QHBoxLayout>
#include <QPainter>

#include "number-edit.h"
#include "scale-bar.h"

namespace Linea::UI {

SpinScale::SpinScale(QWidget* parent)
    : QWidget(parent) {
    construct();
    setRange(0.0, 100.0); // defaults
    setValue(0.0); // Initialize value to ensure synchronization
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(22); // Ensure minimum height for the widget
}

void SpinScale::construct() {
    setObjectName("SpinScale");

    setAttribute(Qt::WA_StyledBackground, true);

    _layout = new QHBoxLayout(this);
    _layout->setContentsMargins(5, 0, 2, 0);
    _layout->setSpacing(2);

    _scaleBar = new ScaleBar(this);
    _scaleBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    _layout->addWidget(_scaleBar, 1);

    _numberEdit = new NumberEdit(this);
    _numberEdit->setDecimals(0);
    _numberEdit->setHasFrame(false);
    _numberEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    _layout->addWidget(_numberEdit);

    // Connect signals to keep scale and spin in sync
    connect(_numberEdit, &NumberEdit::valueChanged, this, [this](double value) {
        syncValueToScale(value);
        Q_EMIT valueChanged(value);
    });

    connect(_scaleBar, &ScaleBar::valueChanged, this, [this](double value) {
        syncValueToSpin(value);
        Q_EMIT valueChanged(value);
    });
}

double SpinScale::value() const {
    return _numberEdit->value();
}

void SpinScale::setValue(double value) {
    _numberEdit->setValue(value);
    syncValueToScale(value);
}

void SpinScale::setValue(const Linea::mixed_property<QVariant>& val) {
    _numberEdit->setValue(val);
    syncValueToScale(val.value().toDouble());
    _scaleBar->setMixedMode(val.is_mixed());
}

void SpinScale::setRange(double min, double max) {
    _numberEdit->setRange(min, max);
    _scaleBar->setRange(min, max);
}

double SpinScale::minimum() const { return _numberEdit->minimum(); }
void SpinScale::setMinimum(double min) {
    _numberEdit->setMinimum(min);
    _scaleBar->setMinimum(min);
}

double SpinScale::maximum() const { return _numberEdit->maximum(); }
void SpinScale::setMaximum(double max) {
    _numberEdit->setMaximum(max);
    _scaleBar->setMaximum(max);
}

QString SpinScale::suffix() const { return _numberEdit->suffix(); }
void SpinScale::setSuffix(const QString& suffix) {
    _numberEdit->setSuffix(suffix);
}

void SpinScale::setPrefix(const QString& prefix) {
    _numberEdit->setPrefix(prefix);
}

int SpinScale::decimals() const { return _numberEdit->decimals(); }
void SpinScale::setDecimals(int digits) {
    _numberEdit->setDecimals(digits);
}

double SpinScale::singleStep() const { return _numberEdit->singleStep(); }
void SpinScale::setSingleStep(double step) {
    _numberEdit->setSingleStep(step);
}

double SpinScale::factor() const { return _numberEdit->factor(); }
void SpinScale::setFactor(double factor) {
    _numberEdit->setFactor(factor);
}

void SpinScale::setMaxBlockCount(int count) {
    _maxBlockCount = count;
    _scaleBar->setMaxBlockCount(count);
}

void SpinScale::setBlockHeight(int height) {
    _scaleBar->setBlockHeight(height);
}

void SpinScale::syncValueToScale(double value) {
    // Block signals to prevent feedback loop
    QSignalBlocker blocker(_scaleBar);
    _scaleBar->setValue(value);
    _scaleBar->update(); // Force repaint
}

void SpinScale::syncValueToSpin(double value) {
    // Block signals to prevent feedback loop
    QSignalBlocker blocker(_numberEdit);
    _numberEdit->setValue(value);
}

void SpinScale::setMixedMode(bool mixed) {
    _scaleBar->setMixedMode(mixed);
    _numberEdit->setMixedMode(mixed);
}

QSize SpinScale::sizeHint() const {
    auto hint = QWidget::sizeHint();
    // The default sizeHint is driven by ScaleBar (100px) + NumberEdit
    // (sized to fit textFromValue(max), which can be huge). Cap the width
    // so SpinScale doesn't force LPE panels to be excessively wide.
    constexpr int width = 130;
    if (hint.width() > width) hint.setWidth(width);
    return hint;
}

} // namespace Linea::UI
