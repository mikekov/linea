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
#include <QGridLayout>
#include <QGuiApplication>
#include <QIcon>
#include <QLayout>
#include <QLinearGradient>
#include <QPainter>
#include <QObject>
#include <QPointer>
#include <QLineEdit>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVariantAnimation>
#include <QWidget>
#include <QWindow>
#include <algorithm>
#include <string>

#include "expander.h"
#include "preferences.h"
#include "qt/util/motion.h"

namespace Linea::UI {

namespace {

// Event filter installed on the leader widgets; mirrors their combined
// Show/Hide state onto the follower (visible while any leader is).
// Parented to the follower so it dies with it.
class VisibilitySyncFilter : public QObject {
public:
    VisibilitySyncFilter(QWidget* follower, std::vector<QWidget*> leaders)
        : QObject(follower)
        , _follower(follower) {
        for (auto leader : leaders) {
            if (!leader) continue;
            _leaders.push_back(leader);
            leader->installEventFilter(this);
        }
    }

protected:
    bool eventFilter(QObject* /*watched*/, QEvent* event) override {
        if (event->type() == QEvent::Show || event->type() == QEvent::Hide) {
            _follower->setVisible(anyVisible());
        }
        return false;
    }

private:
    bool anyVisible() const {
        for (auto leader : _leaders) {
            if (leader && leader->isVisible()) return true;
        }
        return false;
    }

    QWidget* _follower;
    std::vector<QPointer<QWidget>> _leaders;
};

// QLineEdit embeds addAction() icons as private QLineEditIconButton children
// (a QToolButton subclass) whose paintEvent draws only the icon — no style
// primitives, so stylesheets can't give it a hover state, and its opaque
// paint path skips autoFillBackground fills. The hover tint is therefore
// painted here during the button's Paint event, before the icon draws.
// Parented to the button so it dies with it.
class IconHoverFilter : public QObject {
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        auto w = static_cast<QWidget*>(watched);
        if (event->type() == QEvent::Paint) {
            if (_bg.alpha() > 0) {
                QPainter p(w);
                p.fillRect(w->rect(), _bg);
            }
            // returning false lets paintEvent draw the icon on top
        } else if (event->type() == QEvent::Enter || event->type() == QEvent::Leave) {
            if (event->type() == QEvent::Enter &&
                (!w->isEnabled() || (w->parentWidget() && !w->parentWidget()->isEnabled())))
                return false;
            QColor target = w->palette().color(QPalette::Midlight);
            target.setAlpha(event->type() == QEvent::Enter ? 255 : 0);
            // Start from the target's RGB at the current alpha — Qt::transparent
            // is black under the hood and interpolating its channels flashes dark.
            QColor start = target;
            start.setAlpha(_bg.alpha());
            auto anim = new QVariantAnimation(this);
            anim->setDuration(160);
            anim->setStartValue(start);
            anim->setEndValue(target);
            connect(anim, &QVariantAnimation::valueChanged, this, [w, this](const QVariant& v) {
                _bg = v.value<QColor>();
                w->update();
            });
            anim->start(QAbstractAnimation::DeleteWhenStopped);
        } else if (event->type() == QEvent::CursorChange) {
            // QLineEditIconButton::updateCursor() re-sets the cursor on every
            // setOpacity — keep re-asserting the pointing hand instead.
            if (w->cursor().shape() != Qt::PointingHandCursor)
                w->setCursor(Qt::PointingHandCursor);
        }
        return false;
    }

private:
    QColor _bg{Qt::transparent};
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

ScrollBarAutoHide::ScrollBarAutoHide(QScrollArea* scrollArea, Mode mode)
    : QObject(scrollArea)
    , _scrollArea(scrollArea)
    , _mode(mode) {
    auto bar = scrollArea->verticalScrollBar();
    if (_mode == Overlay) {
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        _overlayBar = new QScrollBar(Qt::Vertical, scrollArea);
        _overlayBar->setCursor(Qt::ArrowCursor);
        _overlayBar->setAttribute(Qt::WA_OpaquePaintEvent, false);
        _overlayBar->hide();
        _overlayBar->installEventFilter(this);

        connect(bar, &QScrollBar::valueChanged, this, [this, bar](int value) {
            QSignalBlocker blocker(_overlayBar);
            _overlayBar->setRange(bar->minimum(), bar->maximum());
            _overlayBar->setValue(value);
        });
        connect(_overlayBar, &QScrollBar::valueChanged, bar, &QScrollBar::setValue);
    } else {
        auto sp = bar->sizePolicy();
        sp.setRetainSizeWhenHidden(true);
        bar->setSizePolicy(sp);
        bar->installEventFilter(this);
    }

    scrollArea->installEventFilter(this);
    scrollArea->viewport()->installEventFilter(this);
    connect(bar, &QScrollBar::rangeChanged, this, [this](int, int) { apply(); });
    apply();
}

void ScrollBarAutoHide::setForceHidden(bool hidden) {
    if (_forceHidden == hidden) return;
    _forceHidden = hidden;
    apply();
}

void ScrollBarAutoHide::scheduleApply() {
    if (_applyScheduled) return;
    _applyScheduled = true;
    QTimer::singleShot(0, this, [this] {
        _applyScheduled = false;
        apply();
    });
}

bool ScrollBarAutoHide::eventFilter(QObject* watched, QEvent* event) {
    const auto type = event->type();
    if (_mode == Overlay && watched == _overlayBar) {
        if (type == QEvent::StyleChange) {
            _overlayBar->setAttribute(Qt::WA_OpaquePaintEvent, false);
            scheduleApply();
        } else if (type == QEvent::ShowToParent) {
            scheduleApply();
        }
        return false;
    }

    if (_mode == Reserved && watched == _scrollArea->verticalScrollBar()) {
        if (type == QEvent::StyleChange || type == QEvent::ShowToParent) {
            scheduleApply();
        }
        return false;
    }

    bool onScrollArea = watched == _scrollArea || watched == _scrollArea->viewport();
    bool layoutEvent = type == QEvent::LayoutRequest || type == QEvent::Resize || type == QEvent::StyleChange;
    if (onScrollArea && layoutEvent) {
        if (_mode == Overlay) {
            updateOverlayGeometry();
        }
        // deferred: the scroll area re-shows the bar while handling this
        // very event; apply after its layout pass completes
        scheduleApply();
    }
    return false;
}

void ScrollBarAutoHide::updateOverlayGeometry() {
    if (!_overlayBar) return;
    _overlayBar->ensurePolished();
    _overlayBar->setAttribute(Qt::WA_OpaquePaintEvent, false);
    const QRect cr = _scrollArea->contentsRect();
    const int w = _overlayBar->sizeHint().width();
    const QRect barRect = QStyle::visualRect(
        _scrollArea->layoutDirection(), cr,
        QRect(cr.right() - w + 1, cr.top(), w, cr.height()));
    _overlayBar->setGeometry(barRect);
    _overlayBar->raise();
}

void ScrollBarAutoHide::apply() {
    auto bar = _scrollArea->verticalScrollBar();
    const bool shouldShow = bar->maximum() > 0 && !_forceHidden;
    if (_mode == Overlay) {
        {
            QSignalBlocker blocker(_overlayBar);
            _overlayBar->setRange(bar->minimum(), bar->maximum());
            _overlayBar->setPageStep(bar->pageStep());
            _overlayBar->setSingleStep(bar->singleStep());
            _overlayBar->setValue(bar->value());
        }
        updateOverlayGeometry();
        _overlayBar->setVisible(shouldShow);
        return;
    }

    if (_scrollArea->verticalScrollBarPolicy() != Qt::ScrollBarAlwaysOn) return;
    auto sp = bar->sizePolicy();
    if (!sp.retainSizeWhenHidden()) {
        sp.setRetainSizeWhenHidden(true);
        bar->setSizePolicy(sp);
    }
    bar->setVisible(shouldShow);
}

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
    new VisibilitySyncFilter(follower, {leader});
}

void syncVisibilityAny(QWidget* follower, std::vector<QWidget*> leaders) {
    if (!follower || leaders.empty()) return;
    follower->setVisible(std::any_of(leaders.begin(), leaders.end(),
        [](QWidget* w) { return w && w->isVisible(); }));
    new VisibilitySyncFilter(follower, std::move(leaders));
}

void fixColumnToolButtons(QWidget* widget, int column, QSize size) {
    if (!widget) return;
    auto* grid = widget->findChild<QGridLayout*>(QStringLiteral("mainGrid"));
    if (!grid) return;
    for (int i = 0; i < grid->count(); ++i) {
        int row, col, rowspan, colspan;
        grid->getItemPosition(i, &row, &col, &rowspan, &colspan);
        if (col != column) continue;
        if (auto* btn = qobject_cast<QToolButton*>(grid->itemAt(i)->widget()))
            btn->setFixedSize(size);
    }
}

void highlightEmbeddedIconsOnHover(QLineEdit* edit) {
    if (!edit || !edit->isEnabled()) return;
    for (auto* button : edit->findChildren<QAbstractButton*>()) {
        button->setCursor(Qt::PointingHandCursor);
        button->installEventFilter(new IconHoverFilter(button));
    }
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
    CollapsibleSectionController(Expander* toggleBtn, std::vector<QWidget*> containers,
                                 const char* prefsPath, bool defaultExpanded)
        : QObject(toggleBtn)
        , _toggleBtn(toggleBtn)
        , _prefsPath(prefsPath ? prefsPath : "")
        , _animation(new QVariantAnimation(this)) {
        _containers.reserve(containers.size());
        for (auto container : containers) {
            _containers.push_back(container);
        }

        _expanded = !_prefsPath.empty()
            ? Inkscape::Preferences::get()->getBool(_prefsPath, defaultExpanded)
            : defaultExpanded;

        for (auto container : _containers) {
            if (container) container->setVisible(_expanded);
        }
        updateToggle();

        _animation->setEasingCurve(QEasingCurve::OutCubic);

        connect(_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            const qreal p = value.toReal();
            for (int i = 0; i < static_cast<int>(_containers.size()); ++i) {
                auto container = _containers[i];
                if (!container) continue;
                container->setFixedHeight(_startHeights[i] + qRound((_endHeights[i] - _startHeights[i]) * p));
            }
            settleLayout();
        });

        connect(_animation, &QVariantAnimation::finished, this, [this]() {
            for (auto container : _containers) {
                if (!container) continue;
                if (!_expanded) {
                    container->setVisible(false);
                }
                container->setMinimumHeight(0);
                container->setMaximumHeight(QWIDGETSIZE_MAX);
            }
            settleLayout();
        });

        connect(_toggleBtn, &Expander::clicked, this, [this]() {
            toggle();
        });
    }

    bool isExpanded() const { return _expanded; }

private:
    QWidget* firstContainer() const {
        for (auto container : _containers) {
            if (container) return container;
        }
        return nullptr;
    }

    void settleLayout() {
        auto container = firstContainer();
        if (!container) return;
        Linea::UI::settleLayout(container->parentWidget() ? container->parentWidget() : container);
    }

    // Push the tracked state into the expander — a no-op after a user click
    // (the expander already flipped itself), seeds it on construction.
    void updateToggle() {
        if (!_toggleBtn) return;
        _toggleBtn->setExpanded(_expanded);
    }

    void toggle() {
        if (!firstContainer()) return;
        _expanded = !_expanded;
        updateToggle();

        if (!_prefsPath.empty()) {
            Inkscape::Preferences::get()->setBool(_prefsPath, _expanded);
        }

        _startHeights.clear();
        _endHeights.clear();
        for (auto container : _containers) {
            if (!container) {
                _startHeights.push_back(0);
                _endHeights.push_back(0);
                continue;
            }
            if (_expanded) {
                _startHeights.push_back(0);
                _endHeights.push_back(container->sizeHint().height());
                container->setFixedHeight(0);
                container->setVisible(true);
            } else {
                _startHeights.push_back(container->height());
                _endHeights.push_back(0);
            }
        }

        _animation->stop();
        _animation->setDuration(effectiveAnimationDuration(200));
        _animation->setStartValue(0.0);
        _animation->setEndValue(1.0);
        _animation->start();
    }

    QPointer<Expander> _toggleBtn;
    std::vector<QPointer<QWidget>> _containers;
    std::vector<int> _startHeights;
    std::vector<int> _endHeights;
    std::string _prefsPath;
    QVariantAnimation* _animation = nullptr;
    bool _expanded = false;
};

} // namespace

std::function<bool()> setupCollapsibleSection(Expander* expander, std::vector<QWidget*> containers,
                                              const char* prefsPath, bool defaultExpanded) {
    if (!expander || containers.empty()) {
        return []() { return false; };
    }
    auto controller = new CollapsibleSectionController(expander, std::move(containers), prefsPath, defaultExpanded);
    QPointer<CollapsibleSectionController> guard(controller);
    return [guard]() { return guard ? guard->isExpanded() : false; };
}

} // namespace Linea::UI
