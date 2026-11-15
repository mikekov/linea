// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * LinkedNumberPair widget - two NumberEdit fields with a link toggle button.
 */

#include "linked-number-pair.h"

#include <QToolButton>

#include "number-edit.h"
#include "ui_linked-number-pair.h"

namespace Linea::UI {

LinkedNumberPair::LinkedNumberPair(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::LinkedNumberPair>()) {
    _ui->setupUi(this);

    connect(_ui->linkButton, &QToolButton::toggled, this, [this](bool linked) {
        if (linked && _copyLinked) {
            _ui->secondEdit->setValue(_ui->firstEdit->value());
        }
        Q_EMIT linkToggled(linked);
    });
    connect(_ui->firstEdit, &NumberEdit::valueChanged, this, [this](double value) {
        if (_copyLinked && _ui->linkButton->isChecked()) {
            _ui->secondEdit->setValue(value);
        }
        Q_EMIT firstChanged(value);
    });
    connect(_ui->secondEdit, &NumberEdit::valueChanged, this, [this](double value) {
        if (_copyLinked && _ui->linkButton->isChecked()) {
            _ui->firstEdit->setValue(value);
        }
        Q_EMIT secondChanged(value);
    });
}

LinkedNumberPair::~LinkedNumberPair() = default;

NumberEdit* LinkedNumberPair::first() const {
    return _ui->firstEdit;
}

NumberEdit* LinkedNumberPair::second() const {
    return _ui->secondEdit;
}

QToolButton* LinkedNumberPair::linkButton() const {
    return _ui->linkButton;
}

bool LinkedNumberPair::isLinked() const {
    return _ui->linkButton->isChecked();
}

void LinkedNumberPair::setLinked(bool linked) {
    _ui->linkButton->setChecked(linked);
}

int LinkedNumberPair::spacing() const {
    return layout()->spacing();
}

void LinkedNumberPair::setSpacing(int spacing) {
    layout()->setSpacing(spacing);
}

double LinkedNumberPair::minimum() const {
    return _ui->firstEdit->minimum();
}

void LinkedNumberPair::setMinimum(double minimum) {
    _ui->firstEdit->setMinimum(minimum);
    _ui->secondEdit->setMinimum(minimum);
}

double LinkedNumberPair::maximum() const {
    return _ui->firstEdit->maximum();
}

void LinkedNumberPair::setMaximum(double maximum) {
    _ui->firstEdit->setMaximum(maximum);
    _ui->secondEdit->setMaximum(maximum);
}

int LinkedNumberPair::decimals() const {
    return _ui->firstEdit->decimals();
}

void LinkedNumberPair::setDecimals(int decimals) {
    _ui->firstEdit->setDecimals(decimals);
    _ui->secondEdit->setDecimals(decimals);
}

double LinkedNumberPair::singleStep() const {
    return _ui->firstEdit->singleStep();
}

void LinkedNumberPair::setSingleStep(double step) {
    _ui->firstEdit->setSingleStep(step);
    _ui->secondEdit->setSingleStep(step);
}

QString LinkedNumberPair::prefix() const {
    return _ui->firstEdit->prefix();
}

void LinkedNumberPair::setPrefix(const QString& prefix) {
    _ui->firstEdit->setPrefix(prefix);
    _ui->secondEdit->setPrefix(prefix);
}

QString LinkedNumberPair::suffix() const {
    return _ui->firstEdit->suffix();
}

void LinkedNumberPair::setSuffix(const QString& suffix) {
    _ui->firstEdit->setSuffix(suffix);
    _ui->secondEdit->setSuffix(suffix);
}

int LinkedNumberPair::keptFractionDigits() const {
    return _ui->firstEdit->keptFractionDigits();
}

void LinkedNumberPair::setKeptFractionDigits(int digits) {
    _ui->firstEdit->setKeptFractionDigits(digits);
    _ui->secondEdit->setKeptFractionDigits(digits);
}

QString LinkedNumberPair::firstIcon() const {
    return _ui->firstEdit->icon();
}

void LinkedNumberPair::setFirstIcon(const QString& icon) {
    _ui->firstEdit->setIcon(icon);
}

QString LinkedNumberPair::secondIcon() const {
    return _ui->secondEdit->icon();
}

void LinkedNumberPair::setSecondIcon(const QString& icon) {
    _ui->secondEdit->setIcon(icon);
}

QString LinkedNumberPair::firstLabel() const {
    return _ui->firstEdit->label();
}

void LinkedNumberPair::setFirstLabel(const QString& label) {
    _ui->firstEdit->setLabel(label);
}

QString LinkedNumberPair::secondLabel() const {
    return _ui->secondEdit->label();
}

void LinkedNumberPair::setSecondLabel(const QString& label) {
    _ui->secondEdit->setLabel(label);
}

QString LinkedNumberPair::firstToolTip() const {
    return _ui->firstEdit->toolTip();
}

void LinkedNumberPair::setFirstToolTip(const QString& tip) {
    _ui->firstEdit->setToolTip(tip);
}

QString LinkedNumberPair::secondToolTip() const {
    return _ui->secondEdit->toolTip();
}

void LinkedNumberPair::setSecondToolTip(const QString& tip) {
    _ui->secondEdit->setToolTip(tip);
}

double LinkedNumberPair::firstValue() const {
    return _ui->firstEdit->value();
}

void LinkedNumberPair::setFirstValue(double value) {
    _ui->firstEdit->setValue(value);
}

double LinkedNumberPair::secondValue() const {
    return _ui->secondEdit->value();
}

void LinkedNumberPair::setSecondValue(double value) {
    _ui->secondEdit->setValue(value);
}

QString LinkedNumberPair::linkToolTip() const {
    return _ui->linkButton->toolTip();
}

void LinkedNumberPair::setLinkToolTip(const QString& tip) {
    _ui->linkButton->setToolTip(tip);
}

} // namespace Linea::UI
