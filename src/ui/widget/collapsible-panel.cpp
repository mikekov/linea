// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Collapsible panel widget implementation.
 */

#include "collapsible-panel.h"
#include "ui/widget/overlay-layout.h"
#include "ui_collapsible-panel.h"
#include "ui/util.h"

#include <QToolButton>
#include <QVBoxLayout>
#include <QIcon>
#include <QStyle>

namespace Linea::UI {

CollapsiblePanel::CollapsiblePanel(QWidget* parent)
    : ResizableEdgeWidget(parent)
    , _ui(std::make_unique<Ui::CollapsiblePanel>())
{
    _ui->setupUi(this);
//_animationDuration=2000;
    _ui->containerWidget->setProperty("class", "overlay-panel");
    _ui->containerWidget->setProperty("rounded", true);
    _ui->headerWidget->setProperty("class", "panel-header");
    // _ui->toggleButton->setProperty("class", "panel-toggle");
    _ui->contentWidget->setProperty("class", "panel-content");

    Inkscape::UI::add_drop_shadow(this);

    _animation = new QPropertyAnimation(this, "maximumHeight", this);
    _animation->setDuration(_animationDuration);
    _animation->setEasingCurve(QEasingCurve::OutQuart);

    // connect(_ui->toggleButton, &QToolButton::clicked, this, &CollapsiblePanel::onToggleClicked);
    connect(_animation, &QPropertyAnimation::finished, this, [this]() {
        if (_collapsed) {
            _ui->contentWidget->hide();
        }
        updateHeight();
        updateGeometry();
    });

    /*
    connect(_animation, &QAbstractAnimation::stateChanged,
        [this](QAbstractAnimation::State newState, QAbstractAnimation::State oldState) {
    if (newState == QAbstractAnimation::Running && oldState == QAbstractAnimation::Stopped) {
        // Animation has started!
   printf("anim start\n");
        updateHeight();
    }
});
*/
    updateToggleButtonIcon();
}

CollapsiblePanel::~CollapsiblePanel() = default;

void CollapsiblePanel::setHeader(QWidget* widget) {
    if (!widget) return;

    // Insert at position 0 (before the spacer and toggle button)
    _ui->headerLayout->insertWidget(0, widget);
}

void CollapsiblePanel::setContentWidget(QWidget* content) {
    if (_ui->contentWidget->layout()) {
        while (_ui->contentWidget->layout()->count() > 0) {
            QLayoutItem* item = _ui->contentWidget->layout()->takeAt(0);
            if (item->widget()) {
                item->widget()->setParent(nullptr);
            }
            delete item;
        }
    }

    if (content) {
        _ui->contentWidget->layout()->addWidget(content);
        content->show();

        // Add spacer at bottom to keep content at top
        // auto* spacer = new QSpacerItem(20, 0, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);
        // _ui->contentWidget->layout()->addItem(spacer);
    }

    if (!_collapsed) {
        updateHeight();
    }
}

QWidget* CollapsiblePanel::contentWidget() const {
    if (_ui->contentWidget->layout() && _ui->contentWidget->layout()->count() > 0) {
        return _ui->contentWidget->layout()->itemAt(0)->widget();
    }
    return nullptr;
}

void CollapsiblePanel::setContentMargins(int left, int top, int right, int bottom) {
    _ui->contentLayout->setContentsMargins(left, top, right, bottom);
}

void CollapsiblePanel::setCollapsed(bool collapsed) {
    if (_collapsed == collapsed) {
        return;
    }

    _collapsed = collapsed;

    if (collapsed) {
        _animation->setStartValue(height());
        _animation->setEndValue(_collapsedHeight);
        _animation->start();
    }
    else {
        _ui->contentWidget->show();

        auto overlay = qobject_cast<OverlayLayout*>(parentWidget()->layout());
        // Measure available height while the maximum is unconstrained
        setMaximumHeight(QWIDGETSIZE_MAX);
        int targetHeight = overlay ? overlay->targetGeometry(this).height() : _collapsedHeight + 200;
        setMaximumHeight(_collapsedHeight);
        setMinimumHeight(_collapsedHeight);

        _animation->setStartValue(_collapsedHeight);
        _animation->setEndValue(targetHeight);
        _animation->start();
    }

    updateToggleButtonIcon();
    Q_EMIT collapsedChanged(_collapsed);
    updateResizeHandle();
}

bool CollapsiblePanel::isCollapsed() const {
    return _collapsed;
}

void CollapsiblePanel::setCollapsedHeight(int height) {
    _collapsedHeight = height;
    if (_collapsed) {
        setMaximumHeight(_collapsedHeight);
    }
}

int CollapsiblePanel::collapsedHeight() const {
    return _collapsedHeight;
}

void CollapsiblePanel::setAnimationDuration(int msecs) {
    _animationDuration = msecs;
    _animation->setDuration(_animationDuration);
}

int CollapsiblePanel::animationDuration() const {
    return _animationDuration;
}

void CollapsiblePanel::setResizeStep(int step) {
    _resizeStep = step;
}

int CollapsiblePanel::resizeStep() const {
    return _resizeStep;
}

void CollapsiblePanel::setRounded(bool rounded) {
    if (_rounded == rounded) {
        return;
    }
    _rounded = rounded;
    _ui->containerWidget->setProperty("rounded", rounded);
    _ui->containerWidget->style()->unpolish(_ui->containerWidget);
    _ui->containerWidget->style()->polish(_ui->containerWidget);
    _ui->containerWidget->update();
}

bool CollapsiblePanel::rounded() const {
    return _rounded;
}

void CollapsiblePanel::onToggleClicked() {
    setCollapsed(!_collapsed);
}

void CollapsiblePanel::updateToggleButtonIcon() {
    // if (_collapsed) {
    //     _ui->toggleButton->setText("▼");
    // } else {
    //     _ui->toggleButton->setText("▲");
    // }
}

void CollapsiblePanel::updateHeight() {
    setMinimumHeight(_collapsedHeight);
    if (!_collapsed) {
        setMaximumHeight(QWIDGETSIZE_MAX);
    } else {
        setMaximumHeight(_collapsedHeight);
    }
}

bool CollapsiblePanel::canResize() const {
    return ResizableEdgeWidget::canResize() && !_collapsed;
}

int CollapsiblePanel::snapResizeWidth(int newWidth) const {
    return ((newWidth + _resizeStep / 2) / _resizeStep) * _resizeStep;
}

} // namespace Linea::UI
