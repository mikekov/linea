// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * GuideWidget implementation
 */

#include "guide-widget.h"

#include "ui_guide-widget.h"
#include "color-button.h"
#include "number-edit.h"
#include <QIcon>
#include <QPushButton>

namespace Linea::UI {

namespace {
    const char* lockIcon(bool locked) {
        return locked ? ":/icons/object-locked" : ":/icons/object-unlocked";
    }
}

GuideWidget::GuideWidget(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::GuideWidget>()) {
    _ui->setupUi(this);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAttribute(Qt::WA_StyledBackground, true);
    _ui->colorButton->setTitle(tr("Guide"));

    connect(_ui->labelEdit, &QLineEdit::textChanged, this, &GuideWidget::onLabelChanged);
    connect(_ui->colorButton, &ColorPicker::colorChanged, this, &GuideWidget::onColorChanged);
    connect(_ui->xEdit, &NumberEdit::valueChanged, this, &GuideWidget::onXChanged);
    connect(_ui->yEdit, &NumberEdit::valueChanged, this, &GuideWidget::onYChanged);
    connect(_ui->angleEdit, &NumberEdit::valueChanged, this, &GuideWidget::onAngleChanged);
    _ui->lockedButton->setIcon(QIcon(lockIcon(false)));
    connect(_ui->lockedButton, &QPushButton::toggled, this, &GuideWidget::onLockedChanged);
    connect(_ui->deleteButton, &QPushButton::clicked, this, &GuideWidget::deleteGuide);
}

GuideWidget::~GuideWidget() = default;

void GuideWidget::setGuideId(const QString& id) {
    _ui->labelEdit->setPlaceholderText(id);
}

QString GuideWidget::label() const {
    return _ui->labelEdit->text();
}

void GuideWidget::setLabel(const QString& label) {
    _ui->labelEdit->setText(label);
}

Inkscape::Colors::Color GuideWidget::color() const {
    return _ui->colorButton->getCurrentColor();
}

void GuideWidget::setColor(const Inkscape::Colors::Color& color) {
    _ui->colorButton->setColor(color);
}

double GuideWidget::x() const {
    return _ui->xEdit->value();
}

double GuideWidget::y() const {
    return _ui->yEdit->value();
}

void GuideWidget::setPosition(double x, double y) {
    _ui->xEdit->setValue(x);
    _ui->yEdit->setValue(y);
}

void GuideWidget::setX(double x) {
    _ui->xEdit->setValue(x);
}

void GuideWidget::setY(double y) {
    _ui->yEdit->setValue(y);
}

double GuideWidget::angle() const {
    return _ui->angleEdit->value();
}

void GuideWidget::setAngle(double angle) {
    _ui->angleEdit->setValue(angle);
}

bool GuideWidget::isLocked() const {
    return _ui->lockedButton->isChecked();
}

void GuideWidget::setLocked(bool locked) {
    _ui->lockedButton->setChecked(locked);
    _ui->lockedButton->setIcon(QIcon(lockIcon(locked)));
}

void GuideWidget::onLabelChanged(const QString& label) {
    Q_EMIT labelChanged(label);
}

void GuideWidget::onColorChanged(const Inkscape::Colors::Color& color) {
    Q_EMIT colorChanged(color);
}

void GuideWidget::onXChanged(double x) {
    Q_EMIT positionChanged(x, y());
}

void GuideWidget::onYChanged(double y) {
    Q_EMIT positionChanged(x(), y);
}

void GuideWidget::onAngleChanged(double angle) {
    Q_EMIT angleChanged(angle);
}

void GuideWidget::onLockedChanged(bool checked) {
    _ui->lockedButton->setIcon(QIcon(lockIcon(checked)));
    Q_EMIT lockedChanged(checked);
}

} // namespace Linea::UI
