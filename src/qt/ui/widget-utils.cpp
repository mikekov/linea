// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Small shared helpers for Qt panel widgets.
 */

#include "widget-utils.h"

#include <QEvent>
#include <QGuiApplication>
#include <QLayout>
#include <QObject>
#include <QPointer>
#include <QScreen>
#include <QTimer>
#include <QWidget>
#include <QWindow>
#include <string>

#include "preferences.h"

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

} // namespace Linea::UI
