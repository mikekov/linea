// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * PathWidget — compact path-data action widget.
 */

#include "path-widget.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPushButton>

#include "point-widget.h"
#include "preferences.h"
#include "popup-menu.h"
#include "props/binder.h"
#include "selection.h"
#include "ui_path-widget.h"

namespace Linea::UI {

namespace {

constexpr QSize kDefaultPopupSize{340, 260};
constexpr QSize kMinimumPopupSize{240, 120};
constexpr QSize kMaximumPopupSize{800, 500};
constexpr auto kPopupWidthPreference = "/dialogs/path/popup-width";
constexpr auto kPopupHeightPreference = "/dialogs/path/popup-height";

} // namespace

PathWidget::PathWidget(QWidget* parent)
    : QWidget(parent)
    , _ui(std::make_unique<Ui::PathWidget>()) {
    _ui->setupUi(this);
    connect(_ui->pathButton, &QPushButton::clicked, this, &PathWidget::onPathButtonClicked);
    _ui->pathLabel->setCursor(Qt::PointingHandCursor);
    _ui->pathLabel->installEventFilter(this);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

PathWidget::~PathWidget() = default;

bool PathWidget::eventFilter(QObject* watched, QEvent* event) {
    if (watched == _ui->pathLabel && event->type() == QEvent::MouseButtonRelease) {
        auto mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            onPathButtonClicked();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void PathWidget::bind(Props::Binder& binder) {
    auto desktop = binder.editor()->desktop();
    if (!desktop) {
        setVisible(false);
        return;
    }

    _desktop = desktop;
    binder.bindField(Props::Field::id, [this](const Props::SelectionState&) { updatePointObject(); });

    // only show when there is a single path, polygon, or polyline selected
    binder.visibleWhen(this, Props::Cond::single<
        &Props::Counts::paths, &Props::Counts::polygons, &Props::Counts::polylines>);
}

void PathWidget::onPathButtonClicked() {
    if (!_desktop) return;

    if (!_popup) {
        _popup = new PopupMenu(this);
        _pointWidget = new PointWidget;
        _popup->setContent(_pointWidget);
        _popup->setResizable(true, kMinimumPopupSize, kMaximumPopupSize);

        auto prefs = Inkscape::Preferences::get();
        const auto popupSize = QSize(
            prefs->getIntLimited(kPopupWidthPreference, kDefaultPopupSize.width(), kMinimumPopupSize.width(),
                                 kMaximumPopupSize.width()),
            prefs->getIntLimited(kPopupHeightPreference, kDefaultPopupSize.height(), kMinimumPopupSize.height(),
                                 kMaximumPopupSize.height()));
        _popup->resize(popupSize);
        connect(_popup, &PopupMenu::resized, this, [](QSize size) {
            auto prefs = Inkscape::Preferences::get();
            prefs->setInt(kPopupWidthPreference, size.width());
            prefs->setInt(kPopupHeightPreference, size.height());
        });
        connect(_pointWidget, &PointWidget::committed, _popup, &PopupMenu::hide);
        connect(_popup, &PopupMenu::popupHidden, _pointWidget, [this] {
            _pointWidget->setObject(nullptr);
        });
    }
    updatePointObject();
    _popup->showLeftOfWidget(_ui->pathButton);
}

void PathWidget::updatePointObject() {
    if (!_pointWidget || !_desktop) return;

    auto selection = _desktop->getSelection();
    _pointWidget->setObject(selection ? selection->single() : nullptr);
}

} // namespace Linea::UI
