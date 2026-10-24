// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Small shared helpers for Qt panel widgets.
 */

#include "widget-utils.h"

#include <QAbstractButton>
#include <QCoreApplication>
#include <QEasingCurve>
#include <QEvent>
#include <QGuiApplication>
#include <QIcon>
#include <QLayout>
#include <QLinearGradient>
#include <QPainter>
#include <QObject>
#include <QPointer>
#include <QScreen>
#include <QTimer>
#include <QVariantAnimation>
#include <QWidget>
#include <QWindow>
#include <algorithm>
#include <string>

#include "preferences.h"
#include "qt/util/motion.h"

namespace Linea::UI {

namespace {

// Event filter installed on the leader widget; mirrors its Show/Hide state
// onto the follower. Parented to the follower so it dies with it.
class VisibilitySyncFilter : public QObject {
public:
    VisibilitySyncFilter(QWidget* follower, QWidget* leader)
        : QObject(follower)
        , _follower(follower)
        , _leader(leader) {
        leader->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject* /*watched*/, QEvent* event) override {
        if (event->type() == QEvent::Show || event->type() == QEvent::Hide) {
            _follower->setVisible(_leader->isVisible());
        }
        return false;
    }

private:
    QWidget* _follower;
    QPointer<QWidget> _leader;
};

// Event filter installed on a top-level window; restores its position and
// size from preferences and persists subsequent move/resize changes under
// `prefsPath` (x, y, width, height). If the saved position lands on a
// display that is not connected — or leaves the title bar off-screen — the
// window is moved back onto a visible screen. Saves are debounced so
// dragging doesn't hammer the prefs file, and flushed on close.
class GeometryPersistenceFilter : public QObject {
public:
    GeometryPersistenceFilter(QWidget* window, std::string prefsPath, QSize defaultSize)
        : QObject(window)
        , _window(window)
        , _prefsPath(std::move(prefsPath)) {
        _window->installEventFilter(this);
        _timer.setSingleShot(true);
        connect(&_timer, &QTimer::timeout, this, &GeometryPersistenceFilter::save);
        restore(defaultSize);
    }

    ~GeometryPersistenceFilter() override {
        _timer.stop();
        save();
    }

protected:
    bool eventFilter(QObject* /*watched*/, QEvent* event) override {
        switch (event->type()) {
            case QEvent::Show:
                _wasShown = true;
                break;
            case QEvent::Move:
            case QEvent::Resize:
                // if (_window->isVisible()) {
                //     _timer.start(SAVE_DELAY_MS);
                // }
                break;
            case QEvent::Hide:
                if (_window->isVisible()) {
                    _timer.start(SAVE_DELAY_MS);
                }
                break;
            case QEvent::Close:
                _timer.stop();
                save();
                break;
            default:
                break;
        }
        return false;
    }

private:
    void save() {
        if (!_wasShown || !_window->isWindow()) return;

        auto prefs = Inkscape::Preferences::get();
        prefs->setInt(_prefsPath + "/x", _window->x());
        prefs->setInt(_prefsPath + "/y", _window->y());
        prefs->setInt(_prefsPath + "/width", _window->width());
        prefs->setInt(_prefsPath + "/height", _window->height());
    }

    void restore(QSize defaultSize) {
        auto prefs = Inkscape::Preferences::get();
        const int w = prefs->getIntLimited(_prefsPath + "/width", 0, 0, 8192);
        const int h = prefs->getIntLimited(_prefsPath + "/height", 0, 0, 8192);
        if (w <= 0 || h <= 0) {
            if (defaultSize.isValid()) {
                _window->resize(defaultSize);
            }
            return;
        }

        const QPoint pos(prefs->getInt(_prefsPath + "/x", 0), prefs->getInt(_prefsPath + "/y", 0));
        _window->resize(w, h);

        // The top-left corner anchors the title bar — if it lands on a
        // connected screen, restore the exact position.
        if (QGuiApplication::screenAt(pos)) {
            _window->move(pos);
            return;
        }

        // The title bar is off-screen. Prefer the display with the largest
        // overlap with the saved rect; if there is none (the saved monitor
        // is gone entirely), fall back to the screen showing the parent.
        const QRect saved(pos, QSize(w, h));
        QScreen* screen = nullptr;
        qint64 bestArea = 0;
        for (auto candidate : QGuiApplication::screens()) {
            const auto overlap = candidate->availableGeometry().intersected(saved);
            const auto area = static_cast<qint64>(overlap.width()) * overlap.height();
            if (area > bestArea) {
                bestArea = area;
                screen = candidate;
            }
        }
        if (!screen) {
            auto parent = _window->parentWidget();
            screen =
                parent && parent->windowHandle() ? parent->windowHandle()->screen() : QGuiApplication::primaryScreen();
        }
        if (!screen) {
            _window->move(pos);
            return;
        }

        const QRect avail = screen->availableGeometry();
        if (bestArea > 0) {
            // Partially off-screen: clamp back onto that display, keeping at
            // least a grab-height strip inside its available geometry.
            _window->move(qBound(avail.left(), pos.x(), avail.right() - qMin(saved.width(), VISIBLE_STRIP)),
                          qBound(avail.top(), pos.y(), avail.bottom() - qMin(saved.height(), VISIBLE_STRIP)));
            return;
        }

        // No overlap with any display — center on the fallback screen.
        _window->move(avail.center() - saved.center());
    }

    // Debounce delay for preference writes during interactive move/resize.
    static constexpr int SAVE_DELAY_MS = 250;
    // Keep at least this much of the window (title bar included) on-screen
    // when clamping a partially off-screen saved position.
    static constexpr int VISIBLE_STRIP = 64;

    QWidget* _window;
    std::string _prefsPath;
    QTimer _timer;
    bool _wasShown = false;
};

} // namespace

void applyHorizontalPadding(QWidget* widget, int left, int right) {
    if (!widget) return;
    if (auto layout = widget->layout()) {
        auto m = layout->contentsMargins();
        m.setLeft(left);
        m.setRight(right);
        layout->setContentsMargins(m);
    }
}

void syncVisibility(QWidget* follower, QWidget* leader) {
    if (!follower || !leader) return;
    follower->setVisible(leader->isVisible());
    new VisibilitySyncFilter(follower, leader);
}

void persistGeometry(QWidget* window, const char* prefsPath, QSize defaultSize) {
    if (!window || !prefsPath) return;
    new GeometryPersistenceFilter(window, prefsPath, defaultSize);
}

void ensurePopupOnScreen(QPoint& pos, QSize size) {
    QScreen* screen = QGuiApplication::screenAt(pos);
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen) {
        return;
    }

    const QRect screenGeometry = screen->availableGeometry();

    // Adjust horizontal position if off-screen
    if (pos.x() < screenGeometry.left()) {
        pos.setX(screenGeometry.left() + 8);
    } else if (pos.x() + size.width() > screenGeometry.right()) {
        pos.setX(screenGeometry.right() - size.width() - 8);
    }

    // Adjust vertical position if off-screen
    if (pos.y() < screenGeometry.top()) {
        pos.setY(screenGeometry.top() + 8);
    } else if (pos.y() + size.height() > screenGeometry.bottom()) {
        pos.setY(screenGeometry.bottom() - size.height() - 8);
    }
}

void paintFadedText(QPainter& painter, const QRect& rect, const QString& text,
                    const QColor& color, Qt::Alignment alignment) {
    if (painter.fontMetrics().horizontalAdvance(text) <= rect.width()) {
        painter.setPen(color);
        painter.drawText(rect, alignment, text);
        return;
    }

    const int fadeWidth = std::min(30, rect.width() / 3);
    auto fadedColor = color;
    fadedColor.setAlpha(0);

    QLinearGradient gradient(rect.right() - fadeWidth, 0, rect.right(), 0);
    gradient.setColorAt(0.0, color);
    gradient.setColorAt(1.0, fadedColor);

    painter.save();
    painter.setClipRect(rect);
    painter.setPen(QPen(QBrush(gradient), 1));
    painter.drawText(rect, alignment, text);
    painter.restore();
}

void settleLayout(QWidget* widget) {
    if (!widget) return;
    if (auto parent = widget->parentWidget()) {
        const auto descendants = parent->findChildren<QWidget*>();
        for (auto d : descendants) {
            QCoreApplication::sendPostedEvents(d, QEvent::Polish);
            QCoreApplication::sendPostedEvents(d, QEvent::FontChange);
            QCoreApplication::sendPostedEvents(d, QEvent::StyleChange);
        }
    }
    for (auto a = widget; a; a = a->parentWidget()) {
        QCoreApplication::sendPostedEvents(a, QEvent::LayoutRequest);
    }
}

namespace {

class CollapsibleSectionController : public QObject {
public:
    CollapsibleSectionController(QAbstractButton* toggleBtn, QWidget* container,
                                 const char* prefsPath, bool defaultExpanded)
        : QObject(container)
        , _toggleBtn(toggleBtn)
        , _container(container)
        , _prefsPath(prefsPath ? prefsPath : "")
        , _animation(new QVariantAnimation(this)) {
        _expanded = !_prefsPath.empty()
            ? Inkscape::Preferences::get()->getBool(_prefsPath, defaultExpanded)
            : defaultExpanded;

        _container->setVisible(_expanded);
        updateIcon();

        _animation->setEasingCurve(QEasingCurve::OutCubic);

        connect(_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            if (!_container) return;
            _container->setFixedHeight(value.toInt());
            settleLayout(_container->parentWidget() ? _container->parentWidget() : _container.data());
        });

        connect(_animation, &QVariantAnimation::finished, this, [this]() {
            if (!_container) return;
            if (!_expanded) {
                _container->setVisible(false);
            }
            _container->setMinimumHeight(0);
            _container->setMaximumHeight(QWIDGETSIZE_MAX);
            settleLayout(_container->parentWidget() ? _container->parentWidget() : _container.data());
        });

        connect(_toggleBtn, &QAbstractButton::clicked, this, [this]() {
            toggle();
        });
    }

    bool isExpanded() const { return _expanded; }

private:
    void updateIcon() {
        if (!_toggleBtn) return;
        _toggleBtn->setIcon(QIcon(_expanded ? ":/icons/section-collapse" : ":/icons/section-expand"));
    }

    void toggle() {
        if (!_container) return;
        _expanded = !_expanded;
        updateIcon();

        if (!_prefsPath.empty()) {
            Inkscape::Preferences::get()->setBool(_prefsPath, _expanded);
        }

        _animation->stop();
        _animation->setDuration(effectiveAnimationDuration(200));
        if (_expanded) {
            int targetHeight = _container->sizeHint().height();
            _container->setFixedHeight(0);
            _container->setVisible(true);
            _animation->setStartValue(0);
            _animation->setEndValue(targetHeight);
        } else {
            _animation->setStartValue(_container->height());
            _animation->setEndValue(0);
        }
        _animation->start();
    }

    QPointer<QAbstractButton> _toggleBtn;
    QPointer<QWidget> _container;
    std::string _prefsPath;
    QVariantAnimation* _animation = nullptr;
    bool _expanded = false;
};

} // namespace

std::function<bool()> setupCollapsibleSection(QAbstractButton* toggleBtn, QWidget* container,
                                              const char* prefsPath, bool defaultExpanded) {
    if (!toggleBtn || !container) {
        return []() { return false; };
    }
    auto controller = new CollapsibleSectionController(toggleBtn, container, prefsPath, defaultExpanded);
    QPointer<CollapsibleSectionController> guard(controller);
    return [guard]() { return guard ? guard->isExpanded() : false; };
}

} // namespace Linea::UI
