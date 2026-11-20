// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Main toolbar implementation with all primary tool buttons.
 */

#include "main-toolbar.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <QCoreApplication>
#include <QGraphicsEffect>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVariantAnimation>
#include "actions/action-registry.h"
#include "overlay-layout.h"
#include "paint-indicator.h"
#include "qt/ui/widget-utils.h"
#include "mask-operations-popup.h"
#include "path-operations-popup.h"
#include "qt/util/motion.h"
#include "ui/util.h"

namespace {
constexpr int EXTRA_ANIM_DURATION_MS = 200;
} // namespace

namespace Linea::UI {

MainToolbar::MainToolbar(QWidget* parent)
    : Toolbar(parent)
{
    if (layout()) {
        layout()->setSizeConstraint(QLayout::SetNoConstraint);
    }
    containerWidget()->setProperty("class", "toolbar-widget");
    containerWidget()->setProperty("rounded", true);
    Inkscape::UI::style_overlay_panel(this, containerWidget());

    auto& a = ActionRegistry::get();

    _current_shape_action = a.action("tool-rect");
    _current_drawing_action = a.action("tool-pen");
    _current_extra_action = a.action("tool-zoom");
    _current_other_action = a.action("tool-eraser");

    addButton("tool-select");
    addButton("tool-node");

    const std::array drawing_tools = {
        "tool-pen",
        "tool-pencil",
        "tool-calligraphy",
    };
    addToolGroup(drawing_tools, _current_drawing_action);

    // Shape tools group (with popup menu)
    const std::array shape_tools = {
        "tool-rect",
        "tool-ellipse",
        "tool-star",
        "tool-polygon",
        "tool-spiral"
    };
    addToolGroup(shape_tools, _current_shape_action);

    addButton("tool-text");

    const std::array other_tools = {
        "-",
        "tool-dropper",
        "tool-eraser",
        "tool-tweak",
        "tool-spray",
        "tool-paintbucket",
        "-",
        "tool-gradient",
        "tool-mesh",
    };
    addToolGroup(other_tools, _current_other_action);

    const std::array extra_tools = {
        "tool-zoom",
        "tool-measure",
        "tool-connector",
    };
    addToolGroup(extra_tools, _current_extra_action);

    // addSpacer();
    _paintIndicator = new Linea::UI::PaintIndicator(this);
    _paintIndicator->setFixedSize(30, 30);
    addWidget(_paintIndicator);

    // Contextual buttons in a slot on the right: shown only when their feature applies.
    // Hidden buttons contribute nothing, so the toolbar keeps its natural width.
    _extraSlot = new QWidget(this);
    // The slot needs a real layout: a plain QWidget's sizeHint is (-1,-1)
    // regardless of setFixedWidth, so the flow layout could never account
    // for it — the toolbar couldn't grow and the slot overflowed the edge.
    // A layout gives the slot a sizeHint derived from its visible children.
    auto extraSlotLayout = new QHBoxLayout(_extraSlot);
    extraSlotLayout->setContentsMargins(0, 0, 0, 0);
    extraSlotLayout->setSpacing(0);
    _extraSlot->setFixedHeight(buttonSize());

    // Dedicated shape-builder toggle — checkable tool action, shown when the
    // builder can operate on the selection.
    _shapeBuilder = createButton("tool-shape-builder");
    _shapeBuilder->setParent(_extraSlot);
    extraSlotLayout->addWidget(_shapeBuilder);

    // Shape-builder accept/cancel push buttons — shown only while the
    // shape-builder tool is active.
    _shapeBuilderOK = createButton("tool-shape-builder-accept");
    _shapeBuilderOK->setParent(_extraSlot);
    extraSlotLayout->addWidget(_shapeBuilderOK);

    _shapeBuilderCancel = createButton("tool-shape-builder-cancel");
    _shapeBuilderCancel->setParent(_extraSlot);
    extraSlotLayout->addWidget(_shapeBuilderCancel);

    // Path-operations popup button — opens the PathOperations panel.
    _pathOps = createPopupButton(QIcon(":/icons/path-operations"), tr("Path operations"),
                                 new PathOperationsPopup);
    _pathOps->setParent(_extraSlot);
    extraSlotLayout->addWidget(_pathOps);

    // Mask-operations popup button — opens the MaskOperations panel.
    _maskOps = createPopupButton(QIcon(":/icons/mask"), tr("Clip and mask operations"),
                                 new MaskOperationsPopup);
    _maskOps->setParent(_extraSlot);
    extraSlotLayout->addWidget(_maskOps);

    // Exit-group-editing push button — shown while editing inside a group.
    _exitGroupEditing = createButton("selection-group-exit");
    _exitGroupEditing->setParent(_extraSlot);
    extraSlotLayout->addWidget(_exitGroupEditing);

    // Gap before the context buttons, same as between tool groups. It's a
    // trailing edge spacer when the slot is hidden, so it costs no width.
    addSpace();
    addWidget(_extraSlot);

    // Full expansion = the slot's sizeHint — children are all still visible
    // here; updateExtraVisibility() hides the ones that don't apply later.
    _extraMaxWidth = _extraSlot->layout()->sizeHint().width();

    setCentered(true);
    // size limits are locked in with the slot at its widest, so the toolbar
    // can still grow to fit every context widget when shown later
    _extraSlot->setFixedWidth(_extraMaxWidth);
    finalizeLayout();
    _maxWidth = maximumWidth();

    _extraSlot->hide();
    _extraSlot->setFixedWidth(0);
    _minWidth = sizeForWidth(1).width();
    setMinimumWidth(_minWidth);
    setMaximumWidth(_maxWidth);

    _extraAnimation = new QVariantAnimation(this);
    _extraAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(_extraAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        applyExtraWidth(value.toInt());
    });
    connect(_extraAnimation, &QVariantAnimation::finished, this, [this]() {
        applyExtraWidth(_extraTargetWidth);
    });
}

void MainToolbar::setShapeBuilderVisible(bool visible) {
    if (_shapeBuilderVisible == visible) return;
    _shapeBuilderVisible = visible;
    updateExtraVisibility();
}

void MainToolbar::setShapeBuilderCancelVisible(bool visible) {
    if (_shapeBuilderCancelVisible == visible) return;
    _shapeBuilderCancelVisible = visible;
    updateExtraVisibility();
}

void MainToolbar::setPathOpsVisible(bool visible) {
    if (_pathOpsVisible == visible) return;
    _pathOpsVisible = visible;
    updateExtraVisibility();
}

void MainToolbar::setMaskOpsVisible(bool visible) {
    if (_maskOpsVisible == visible) return;
    _maskOpsVisible = visible;
    updateExtraVisibility();
}

void MainToolbar::setIsolationExitVisible(bool visible) {
    if (_isolationExitVisible == visible) return;
    _isolationExitVisible = visible;
    updateExtraVisibility();
}

void MainToolbar::updateExtraVisibility() {
    // The slot layout packs visible widgets left-to-right and reports their
    // summed width as the slot's sizeHint.
    _shapeBuilder->setVisible(_shapeBuilderVisible);
    _shapeBuilderOK->setVisible(_shapeBuilderCancelVisible);
    _shapeBuilderCancel->setVisible(_shapeBuilderCancelVisible);
    _pathOps->setVisible(_pathOpsVisible);
    _maskOps->setVisible(_maskOpsVisible);
    _exitGroupEditing->setVisible(_isolationExitVisible);
    const int target = _extraSlot->layout()->sizeHint().width();
    _extraTargetWidth = target;

    const int distance = std::abs(target - _currentExtraWidth);
    const int duration = (distance > 0 && isVisible())
        ? effectiveAnimationDuration(EXTRA_ANIM_DURATION_MS)
        : 0;

    if (duration <= 0) {
        const QSignalBlocker blocker(_extraAnimation);
        _extraAnimation->stop();
        applyExtraWidth(target);
        return;
    }

    {
        const QSignalBlocker blocker(_extraAnimation);
        _extraAnimation->stop();
        _extraAnimation->setDuration(duration);
        _extraAnimation->setStartValue(_currentExtraWidth);
        _extraAnimation->setEndValue(target);
    }
    _extraAnimation->start();
}

void MainToolbar::hideEvent(QHideEvent* event) {
    Toolbar::hideEvent(event);
    if (!event->spontaneous() && _extraAnimation && _extraAnimation->state() == QAbstractAnimation::Running) {
        const QSignalBlocker blocker(_extraAnimation);
        _extraAnimation->stop();
        applyExtraWidth(_extraTargetWidth);
    }
}

void MainToolbar::applyExtraWidth(int width) {
    width = std::clamp(width, 0, _extraMaxWidth);
    const bool showSlot = width > 0;
    if (width == _currentExtraWidth && !_extraSlot->isHidden() == showSlot) {
        return;
    }

    const QRect oldGeom = geometry();
    const bool wasHidden = _extraSlot->isHidden();
    _currentExtraWidth = width;

    if (showSlot) {
        _extraSlot->setFixedWidth(width);
        if (wasHidden) {
            _extraSlot->show();
        }
    } else {
        if (!wasHidden) {
            _extraSlot->hide();
        }
        _extraSlot->setFixedWidth(0);
    }

    auto cw = containerWidget();
    if (cw && cw->layout()) {
        cw->layout()->invalidate();
        cw->layout()->setEnabled(false);
    }

    auto p = parentWidget();
    if (p) {
        if (auto overlay = qobject_cast<OverlayLayout*>(p->layout())) {
            overlay->updatePanelGeometry();
        } else if (p->layout()) {
            p->layout()->activate();
        }
        // Shrinking leaves the drop-shadow halo outside the new geometry —
        // repaint the region the effect covered, not just the widget rect.
        // (that still doesn't work correctly when expanding leading to accumulation)
        if (auto effect = graphicsEffect(); effect && geometry().width() < oldGeom.width()) {
            p->update(effect->boundingRectFor(oldGeom).toAlignedRect());
        }
    }
    if (cw) {
        cw->setGeometry(rect());
        if (cw->layout()) {
            cw->layout()->setEnabled(true);
            cw->layout()->activate();
        }
        cw->update();
    }
    if (showSlot && wasHidden && cw) {
        settleLayout(cw);
    } else {
        if (cw) {
            QCoreApplication::sendPostedEvents(cw, QEvent::LayoutRequest);
        }
        QCoreApplication::sendPostedEvents(this, QEvent::LayoutRequest);
        if (p) {
            QCoreApplication::sendPostedEvents(p, QEvent::LayoutRequest);
        }
    }
}

void MainToolbar::setStretch(bool stretch) {
    if (stretch) {
        setMinimumWidth(30);
        setMaximumWidth(QWIDGETSIZE_MAX);
    } else {
        setMinimumWidth(_minWidth);
        setMaximumWidth(_maxWidth);
    }
}

} // namespace Linea::UI
