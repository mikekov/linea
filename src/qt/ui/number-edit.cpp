// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * NumberEdit widget implementation.
 */

#include "number-edit.h"

#include <QApplication>
#include <QDoubleSpinBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QStyleOption>
#include <QTimer>
#include <QVariantAnimation>
#include <cmath>

#include "util/expression-evaluator.h"
#include "util/numeric/converters.h"

namespace Linea::UI {

// A transparent overlay placed over the spinbox's line edit: while the field
// is unfocused it fades out fraction digits past the first few by painting a
// background-colored gradient over them. Because it is a child widget painted
// after the line edit, the text itself is always drawn by Qt at its native
// position — nothing is repainted or relaid out.
class FadeOverlay : public QWidget {
public:
    explicit FadeOverlay(QLineEdit* edit)
        : QWidget(edit)
        , _edit(edit) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setGeometry(edit->rect());
        edit->installEventFilter(this);
    }

    void setKeptDigits(int n) {
        if (_keptDigits == n) return;
        _keptDigits = n;
        update();
    }

    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == _edit && event->type() == QEvent::Resize) {
            setGeometry(_edit->rect());
        }
        return false;
    }

protected:
    void paintEvent(QPaintEvent*) override {
        if (_edit->hasFocus() || _edit->hasSelectedText()) return;

        QString shown = _edit->displayText();
        int sep = shown.indexOf(locale().decimalPoint());
        if (sep < 0) return;

        // Fraction digits run until the first non-digit after the separator,
        // so a trailing suffix is never covered by the overlay.
        int frac_end = sep + 1;
        while (frac_end < shown.size() && shown[frac_end].isDigit()) {
            ++frac_end;
        }
        int fade_from = sep + 1 + _keptDigits;
        if (fade_from >= frac_end) return;

        QStyleOptionFrame option;
        option.initFrom(_edit);
        QRect contents = _edit->style()->subElementRect(QStyle::SE_LineEditContents, &option, _edit);
        // the line edit clips its text to contents minus the text margins —
        // the erasure must stop at the same bound or the background ramp
        // would wipe the parent's frame where scrolled text can't reach
        const QMargins margins = _edit->textMargins();
        contents.adjust(margins.left(), margins.top(), -margins.right(), -margins.bottom());

        // Locate the text origin from the leftmost visible character:
        // cursorPositionAt reads the line control's real layout, so
        // scrolling and alignment are already accounted for. Coordinates
        // past the edges are fine — the clip rect bounds the painting.
        auto fm = _edit->fontMetrics();
        int y = contents.center().y();
        int anchor = _edit->cursorPositionAt(QPoint(contents.x(), y));
        if (anchor >= frac_end) return; // fraction scrolled fully off-screen

        // cursorPositionAt snaps to the nearest char boundary, so the x
        // where it flips past `anchor` is that character's midpoint.
        int lo = contents.x(), hi = contents.right();
        if (hi < lo) return;

        while (lo < hi) {
            int mid = lo + (hi - lo) / 2;
            if (_edit->cursorPositionAt(QPoint(mid, y)) <= anchor) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }
        int text_x = lo - fm.horizontalAdvance(shown[anchor]) / 2 - fm.horizontalAdvance(shown.left(anchor));
        int fade_x = text_x + fm.horizontalAdvance(shown.left(fade_from));
        int fade_end = text_x + fm.horizontalAdvance(shown.left(frac_end));
        if (fade_end <= fade_x) return;

        auto group = _edit->isEnabled() ? QPalette::Active : QPalette::Disabled;
        // The visible background is painted by the ancestor NumberEdit's
        // stylesheet (palette(base)); the line edit's own Base is unreliable
        // because the QLineEdit stylesheet rule rebuilds its palette from the
        // standard palette, losing themed colors.
        QColor bg;
        for (auto p = _edit->parentWidget(); p; p = p->parentWidget()) {
            if (p->property("class").toString() == "NumberEdit") {
                bg = p->palette().color(group, QPalette::Base);
                break;
            }
        }
        if (!bg.isValid()) bg = palette().color(group, QPalette::Base);

        // The ramp spans a fixed pixel width so the k-th faded digit maps
        // to the same gradient slice no matter how many digits the
        // fraction has — otherwise a lone faded digit would swallow the
        // whole ramp. Pad spread keeps everything past ramp_x fully masked.
        constexpr int ramp_chars = 3;
        int ramp_x = fade_x + ramp_chars * fm.horizontalAdvance(u'0');

        // Ease-out: alpha rises quickly then settles to opaque, so the
        // fraction tail vanishes fast instead of smearing through a long
        // half-transparent middle.
        auto ease = [](double t) { return 1.0 - std::pow(1.0 - t, 2.0); };
        QLinearGradient gradient(fade_x, 0, ramp_x, 0);
        constexpr int fade_stops = 8;
        for (int i = 0; i <= fade_stops; ++i) {
            const double t = double(i) / fade_stops;
            QColor c = bg;
            c.setAlphaF(ease(t));
            gradient.setColorAt(t, c);
        }

        QPainter painter(this);
        painter.setClipRect(contents);
        painter.fillRect(QRect(fade_x, contents.top(), fade_end - fade_x, contents.height()), gradient);
    }

private:
    QLineEdit* _edit;
    int _keptDigits = 0;
};

// width of the hover-only step arrow zones at the spinbox edges
constexpr int ARROW_W = 16;
// text clip inset: the line edit spans the whole field edge-to-edge, so
// scrolled/overflowing text and the fade overlay's erasure would paint
// right over the parent's 1px frame — keep them a couple pixels inside
constexpr int EDGE_INSET = 2;
// side of the square the pan icon is rendered into, centered in the zone
constexpr int ARROW_ICON = 16;

// hover arrow width adapts to the field's width: full at 60px and wider,
// shrinking one pixel at a time to half width at 40px and narrower
static int arrowWidth(int fieldW) {
    return qBound(ARROW_W / 2,
                  ARROW_W / 2 + (fieldW - 40) * (ARROW_W / 2) / 20,
                  ARROW_W);
}

// crossfade tempo bound to the hover transition, not to a widget's fade
// direction: everything moves fast on hover-enter, slow on hover-leave —
// the label overlaps the left arrow, so a slow label fade-out would read
// as a slow arrow fade-in
constexpr int HOVER_FADE_IN_MS = 70;
constexpr int HOVER_FADE_OUT_MS = 160;
// the label reappears after the arrows have left — slower still, so the
// field settles back to idle instead of everything snapping home at once
constexpr int LABEL_RETURN_MS = 300;

namespace {

// Gradual show/hide: animate a paint-time opacity, hiding the widget for
// real only once a fade-out completes. A QGraphicsOpacityEffect would cache
// the widget's render and can keep a stale blank frame long after the
// widget becomes visible — painting with painter opacity renders live every
// frame. Calls with the same target are no-ops.
class PaintFade {
public:
    explicit PaintFade(QWidget* w, qreal initial = 0.0)
        : _w(w)
        , _fade(initial) {
        QObject::connect(&_anim, &QVariantAnimation::valueChanged, w, [this](const QVariant& v) {
            _fade = v.toDouble();
            _w->update();
        });
        QObject::connect(&_anim, &QVariantAnimation::finished, w, [this] {
            if (!_showing) _w->setVisible(false);
        });
    }

    void fade(bool show, int durationMs) {
        if (!_w->window()->isVisible()) { // nothing on screen to animate from
            _anim.stop();
            _showing = show;
            _fade = show ? 1.0 : 0.0;
            _w->setVisible(show);
            return;
        }
        if (_showing == show) return;
        _showing = show;
        _anim.stop();
        _anim.setDuration(durationMs);
        _anim.setStartValue(_fade);
        _anim.setEndValue(show ? 1.0 : 0.0);
        if (show) _w->setVisible(true);
        _anim.start();
    }

    qreal opacity() const { return _fade; }

private:
    QWidget* _w;
    bool _showing = false;
    qreal _fade;
    QVariantAnimation _anim;
};

// The indicator label with a paint-time fade — same backing, alignment and
// content drawing as QLabel's own paintEvent, just at _fader.opacity().
class IndicatorLabel : public QLabel {
public:
    using QLabel::QLabel;

    void fade(bool show, int durationMs) { _fader.fade(show, durationMs); }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setOpacity(_fader.opacity());
        p.setFont(font());
        QStyleOption opt;
        opt.initFrom(this);
        style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
        // lay out the text line exactly like QLineEdit::paintEvent —
        // (h - fmHeight + 1)/2 rounds the offset up where drawItemText's
        // own centering truncates
        const QFontMetrics fm(font());
        const int fmHeight = qMax(fm.boundingRect(text()).height(), fm.height());
        QRect cr = contentsRect();
        cr.setY(cr.y() + (cr.height() - fmHeight + 1) / 2);
        cr.setHeight(fmHeight);
        if (const QPixmap pm = pixmap(); !pm.isNull()) {
            style()->drawItemPixmap(&p, cr, alignment(), pm);
        } else {
            // the line edit anchors its text line at lineRect.top —
            // baseline = top + fm.ascent(); centering the text's bounding
            // box in the line renders caps-only text ~1px too low
            style()->drawItemText(&p, cr, (alignment() & ~Qt::AlignVertical_Mask) | Qt::AlignTop,
                                  palette(), isEnabled(), text());
        }
    }

private:
    PaintFade _fader{this, 1.0};
};

} // namespace

// Step arrow shown on hover: a floating child painted over the spinbox edge.
// Real up/down buttons are style subcontrols that always reserve space in the
// edit field, even when their arrows are invisible — an overlay costs nothing
// when hidden.
class ArrowOverlay : public QWidget {
public:
    ArrowOverlay(QDoubleSpinBox* spin, int steps, QWidget* parent)
        : QWidget(parent)
        , _spin(spin)
        , _steps(steps) {
        // first timeout is the auto-repeat delay; subsequent ones repeat
        connect(&_repeat, &QTimer::timeout, this, [this] {
            _spin->stepBy(_steps);
            _repeat.setInterval(REPEAT_MS);
        });
    }

    void fade(bool show, int durationMs) { _fader.fade(show, durationMs); }

    // a Leave on the overlay can be missed when it hides under the cursor —
    // the field clears this whenever its own hover ends
    void resetHover() { _hovered = false; update(); }

protected:
    void paintEvent(QPaintEvent*) override {
        static const QIcon left(":/icons/pan-left"), right(":/icons/pan-right");
        QPainter p(this);
        p.setOpacity(_fader.opacity());
        p.setRenderHint(QPainter::Antialiasing);
        // opaque backing so field text never collides with the glyph —
        // rounded on the outer corners to hug the parent's frame
        constexpr int R = 3; // frame radius minus its 1px border
        QPainterPath path;
        const QRectF r = rect();
        path.addRoundedRect(_steps < 0 ? r.adjusted(0, 0, R, 0)
                                       : r.adjusted(-R, 0, 0, 0), R, R);
        p.fillPath(path, palette().color(_hovered ? QPalette::Midlight : QPalette::Base));
        const int s = ARROW_ICON;
        const QRect iconRect((width() - s) / 2, (height() - s) / 2, s, s);
        (_steps < 0 ? left : right).paint(&p, iconRect, Qt::AlignCenter);
    }

    void enterEvent(QEnterEvent*) override { _hovered = true; update(); }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            _spin->stepBy(_steps);
            _repeat.start(REPEAT_DELAY_MS);
        }
    }

    void mouseReleaseEvent(QMouseEvent*) override { _repeat.stop(); }
    void leaveEvent(QEvent*) override { _repeat.stop(); _hovered = false; update(); }

private:
    static constexpr int REPEAT_DELAY_MS = 400;
    static constexpr int REPEAT_MS = 80;

    QDoubleSpinBox* _spin;
    int _steps;
    bool _hovered = false;
    QTimer _repeat;
    PaintFade _fader{this};
};

// Internal spinbox class to access protected lineEdit() and handle formatting
class NumberEdit::SpinBox : public QDoubleSpinBox {
public:
    explicit SpinBox(NumberEdit* parentEdit, QWidget* parent = nullptr)
        : QDoubleSpinBox(parent)
        , _parentEdit(parentEdit) {
        setKeyboardTracking(false);
        setAlignment(Qt::AlignCenter);
    }

    void setTrimZeros(bool trim) { _trimZeros = trim; }
    bool trimZeros() const { return _trimZeros; }

    void setEvaluator(std::function<double(const QString&)> evaluator) { _evaluator = std::move(evaluator); }

    QLineEdit* getLineEdit() const { return lineEdit(); }

    QSize sizeHint() const override {
        return sizedHint([this] { return QDoubleSpinBox::sizeHint(); }, true);
    }
    QSize minimumSizeHint() const override {
        return sizedHint([this] { return QDoubleSpinBox::minimumSizeHint(); }, false);
    }

protected:
    QString textFromValue(double value) const override {
        if (_parentEdit->_mixedMode) {
            return "…";
        }
        // While computing size hints, use the full (untrimmed) representation
        // so the field is sized to fit e.g. "5.000" rather than the trimmed "5".
        if (!_trimZeros || _sizing) {
            return QDoubleSpinBox::textFromValue(value);
        }
        QString text = Inkscape::Util::formatTrimmed(value, decimals());
        // An unfocused field that can't show the full precision drops
        // fraction digits until the text fits — a faded tail still occupies
        // layout width and pushes visible digits off the field's edge.
        auto le = lineEdit();
        if (!le || le->hasFocus()) {
            return text;
        }
        const auto margins = le->textMargins();
        const int avail = contentsRect().width() - margins.left() - margins.right() - 4;
        if (avail <= 0) return text; // no real layout yet — don't trim blindly
        const QFontMetrics fm(le->fontMetrics());
        while (fm.horizontalAdvance(text) > avail) {
            const int dot = text.indexOf(locale().decimalPoint());
            if (dot < 0) break;           // even the integer part overflows
            if (text.endsWith(locale().decimalPoint())) { text.chop(1); break; }
            if (!text.back().isDigit()) break; // a suffix — can't trim safely
            text.chop(1);                 // drop one fraction digit
        }
        return text;
    }

    QValidator::State validate(QString& text, int& pos) const override {
        auto state = QDoubleSpinBox::validate(text, pos);
        if (state != QValidator::Invalid || !_evaluator || _parentEdit->_mixedMode || text.trimmed().isEmpty()) {
            return state;
        }
        // With an evaluator installed, accept expressions and unit suffixes:
        // evaluatable input is Acceptable, anything else Intermediate so it
        // can still be typed but won't commit.
        QString stripped = stripPrefixSuffix(text);
        try {
            _evaluator(stripped);
            return QValidator::Acceptable;
        } catch (...) {
            return QValidator::Intermediate;
        }
    }

    double valueFromText(const QString& text) const override {
        if (_evaluator) {
            // QDoubleSpinBox::valueFromText re-validates internally and
            // returns minimum() for input its own validator rejects — e.g.
            // more fraction digits than decimals() — so parse ourselves for
            // anything beyond strictly-native input.
            QString stripped = stripPrefixSuffix(text);
            bool numberOk = false;
            // parse using C-locale first, then fall back to system locale
            double value = stripped.toDouble(&numberOk);
            if (!numberOk) {
                value = locale().toDouble(stripped, &numberOk);
            }
            if (numberOk) {
                return value;
            }
            try {
                return _evaluator(stripped);
            } catch (...) {
                // Fall through to native parsing only for text Qt accepts
                // itself (e.g. specialValueText)
            }
        }
        // Malformed input must never collapse to minimum()/maximum() —
        // return the unchanged value so interpretText() becomes a no-op.
        int pos = 0;
        QString copy = text;
        if (QDoubleSpinBox::validate(copy, pos) == QValidator::Acceptable) {
            return QDoubleSpinBox::valueFromText(text);
        }
        return value();
    }

    void resizeEvent(QResizeEvent* event) override {
        QDoubleSpinBox::resizeEvent(event);
        fixEditField();
    }

    void changeEvent(QEvent* event) override {
        QDoubleSpinBox::changeEvent(event);
        if (event->type() == QEvent::StyleChange) fixEditField();
    }

    // with NoButtons the style computes a 1px-wide edit field rect —
    // lay the line edit out over the whole contents area ourselves
    void fixEditField() {
        if (buttonSymbols() == QAbstractSpinBox::NoButtons && lineEdit()) {
            lineEdit()->setGeometry(contentsRect());
        }
        // The fitted text depends on the field's width — refresh it on resize.
        // updateDisplay() is private, so compose the string the way it does.
        if (!lineEdit() || lineEdit()->hasFocus()) return;
        QString text;
        if (!specialValueText().isEmpty() && value() == minimum()) {
            text = specialValueText();
        } else {
            text = prefix() + textFromValue(value()) + suffix();
        }
        if (lineEdit()->text() != text) {
            lineEdit()->setText(text);
        }
    }

    void stepBy(int steps) override {
        auto mods = QGuiApplication::queryKeyboardModifiers();
        if (mods & Qt::ControlModifier) {
            // Ctrl key slows down increment/decrement and increases precision
            setValue(value() + steps * singleStep() / 10.0);
            return;
        }
        // Shift speeds up increment/decrement by 10x
        if (mods & Qt::ShiftModifier) steps *= 10;
        QDoubleSpinBox::stepBy(steps);
    }

private:
    QString stripPrefixSuffix(const QString& text) const {
        QString result = text.trimmed();
        QString p = prefix();
        QString s = suffix();
        if (!p.isEmpty() && result.startsWith(p)) {
            result.remove(0, p.length());
        }
        if (!s.isEmpty() && result.endsWith(s)) {
            result.chop(s.length());
        }
        return result.trimmed();
    }

    // static QString formatTrimmed(double value, int prec) {
    //     QString text = QString::number(value, 'f', prec);
    //     if (prec <= 0) {
    //         return text;
    //     }
    //     int end = text.length() - 1;
    //     while (end >= 0 && text[end] == '0') {
    //         end--;
    //     }
    //     if (end >= 0 && text[end] == '.') {
    //         end--;
    //     }
    //     if (end < 0) {
    //         return "0";
    //     }
    //     return text.left(end + 1);
    // }

    NumberEdit* _parentEdit = nullptr;
    bool _trimZeros = true;
    std::function<double(const QString&)> _evaluator;
    // True while computing a size hint, so textFromValue returns the full
    // (untrimmed) text and the field is sized for the widest possible value.
    mutable bool _sizing = false;

    template <typename F>
    QSize sizedHint(F&& base, bool ideal) const {
        _sizing = true;
        QSize hint = base();
        _sizing = false;
        if (ideal) {
            // the base hint covers the text only — reserve fixed chrome on
            // each side: the label zone (or the left arrow) left, the arrow
            // right. Never read the live text margins here — they are set
            // from the laid-out width, so a hint that consumed them would
            // feed back into the layout and freeze a different width per
            // field depending on which margin was current when queried
            int leftZone = ARROW_W;
            if (_parentEdit->_showIndicator) {
                const int labelRight = 2 // indicator x
                    + qBound(_parentEdit->_indicator->minimumWidth(),
                             _parentEdit->_indicator->sizeHint().width(),
                             _parentEdit->_indicator->maximumWidth())
                    + 2; // gap after the label, same as updateTextMargins
                leftZone = qMax(ARROW_W, labelRight);
            }
            hint.rwidth() += ARROW_W + leftZone;
        }
        return hint;
    }
};

NumberEdit::NumberEdit(QWidget* parent)
    : QWidget(parent)
    , _trimZeros(true) {
    setObjectName("NumberEdit");
    setProperty("class", "NumberEdit"); // For stylesheet selection
    setProperty("frame", _hasFrame);
    setAttribute(Qt::WA_StyledBackground, true);

    _layout = new QHBoxLayout(this);
    _layout->setContentsMargins(0, 0, 0, 0);
    _layout->setSpacing(0);

    _spinBox = new NumberEdit::SpinBox(this, this);
    _spinBox->setObjectName("spinBox");
    _spinBox->setFrame(false);
    // real buttons would permanently reserve ~28px of field width
    _spinBox->setButtonSymbols(QAbstractSpinBox::NoButtons);
    _spinBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    _spinBox->setMinimumWidth(18);
    _layout->addWidget(_spinBox, 1);

    // hover-only step arrows, positioned in the Resize filter below; as
    // descendants they keep the field's enter/leave chain intact while
    // hovered — Qt doesn't clip children to the parent's rect
    _arrowDown = new ArrowOverlay(_spinBox, -1, _spinBox);
    _arrowUp = new ArrowOverlay(_spinBox, 1, _spinBox);
    _arrowDown->setVisible(false);
    _arrowUp->setVisible(false);

    // The indicator floats above the spinbox (not in the layout), so hiding
    // it doesn't shift the field; it is hidden while the arrows are visible.
    _indicator = new IndicatorLabel(_spinBox);
    _indicator->setObjectName("indicator");
    _indicator->setAttribute(Qt::WA_StyledBackground, true); // fill its stylesheet background
    _indicator->setAlignment(Qt::AlignCenter);
    _indicator->setAttribute(Qt::WA_TransparentForMouseEvents);
    _indicator->setVisible(false);
    // fix label's width to lock the number box position
    _indicator->setMinimumWidth(14);
    _indicator->setMaximumWidth(30);
    _indicator->raise();
    // the arrows must sit above the label — while it fades out over the
    // left arrow, an opaque cell on top would hide the arrow's own fade-in
    _arrowDown->raise();
    _arrowUp->raise();

    _spinBox->installEventFilter(this);
    _indicator->installEventFilter(this); // late auto-size on show recenters it
    installEventFilter(this); // own Resize also repositions the arrows
    _arrowDown->installEventFilter(this); // landing on an arrow affirms hover
    _arrowUp->installEventFilter(this);
    if (auto le = static_cast<NumberEdit::SpinBox*>(_spinBox)->getLineEdit()) {
        le->installEventFilter(this);
        _fadeOverlay = new FadeOverlay(le);
        connect(le, &QLineEdit::textChanged, this, [this] { updateTextMargins(); });
    }

    // Use old-style macros to avoid assertObjectType runtime check across library boundaries
    connect(_spinBox, SIGNAL(valueChanged(double)), this, SLOT(onSpinBoxValueChanged(double)));
    connect(_spinBox, SIGNAL(editingFinished()), this, SIGNAL(editingFinished()));

    setEvaluator([](const QString& text) {
        const QByteArray utf8 = text.toUtf8();
        // TODO: handle locale specific input
        return Inkscape::Util::ExpressionEvaluator{utf8.constData()}.evaluate().value;
    });
}

NumberEdit::~NumberEdit() = default;

QString NumberEdit::label() const {
    return _indicator->text();
}

void NumberEdit::setHovered(bool hovered) {
    // disabled fields never show the hover arrows
    if (hovered && (!isEnabled() || !_spinBox->isEnabled())) return;

    if (_spinBox->property("hovered").toBool() == hovered) return;

    _spinBox->setProperty("hovered", hovered);
    _spinBox->style()->unpolish(_spinBox);
    _spinBox->style()->polish(_spinBox);
    updateIndicatorVisibility();
    const int ms = hovered ? HOVER_FADE_IN_MS : HOVER_FADE_OUT_MS;
    _arrowDown->fade(hovered, ms);
    _arrowUp->fade(hovered, ms);
    if (!hovered) { // a Leave on an overlay can be missed entirely
        _arrowDown->resetHover();
        _arrowUp->resetHover();
    }
}

void NumberEdit::updateIndicatorVisibility() {
    // the label overlaps the left arrow — when hover toggles, it must move
    // at the hover tempo, not its own fade direction
    const bool hovered = _spinBox->property("hovered").toBool();
    static_cast<IndicatorLabel*>(_indicator)
        ->fade(_showIndicator && !hovered,
               hovered ? HOVER_FADE_IN_MS : LABEL_RETURN_MS);
}

void NumberEdit::updateTextMargins() {
    auto le = static_cast<NumberEdit::SpinBox*>(_spinBox)->getLineEdit();
    int left = 0;
    if (_showIndicator && le) {
        // the label's right edge in the line edit's coordinate space
        const int labelRight = _indicator->x() - le->x()
            + qBound(_indicator->minimumWidth(), _indicator->sizeHint().width(),
                     _indicator->maximumWidth()) + 2;
        const int textW = le->fontMetrics().horizontalAdvance(le->displayText());
        if ((le->width() - textW) / 2 >= labelRight) {
            // comfortable: centered text clears the label zone
            left = qMax(labelRight - arrowWidth(width()), 0);
        } else {
            // tight: the margin keeps the text fully right of the label —
            // it trims to fit and can never underlap, so the label stays
            left = labelRight;
        }
    }
    if (le) {
        le->setTextMargins(qMax(left, EDGE_INSET), 0, EDGE_INSET, 0);
    }
    updateIndicatorVisibility();
    _spinBox->updateGeometry(); // the label's footprint feeds the size hint
}

void NumberEdit::setLabel(const QString& text) {
    _indicator->clear();
    _indicator->setText(text);
    // _indicator->setPixmap(QPixmap());
    _showIndicator = !text.isEmpty();
    updateIndicatorVisibility();
    updateTextMargins();
}

QString NumberEdit::icon() const {
    // Return empty string if no icon is set
    // Could store the icon name if needed, but for now this is a simple implementation
    return QString();
}

void NumberEdit::setIcon(const QString& iconName) {
    setIcon(QIcon(":/icons/" + iconName));
}

void NumberEdit::setIcon(const QIcon& icon) {
    _indicator->setPixmap(icon.pixmap(16, 16));
    _indicator->setText(QString());
    _showIndicator = !icon.isNull();
    updateIndicatorVisibility();
    updateTextMargins();
}

void NumberEdit::clearIndicator() {
    _indicator->setText(QString());
    _indicator->setPixmap(QPixmap());
    _showIndicator = false;
    updateIndicatorVisibility();
    updateTextMargins();
}

void NumberEdit::setTrimZeros(bool trim) {
    _trimZeros = trim;
    static_cast<NumberEdit::SpinBox*>(_spinBox)->setTrimZeros(trim);
    _spinBox->setValue(_spinBox->value()); // Refresh display
}

void NumberEdit::setFadeFraction(bool fade) {
    _fadeFraction = fade;
    if (_fadeOverlay) {
        _fadeOverlay->setVisible(fade);
    }
}

void NumberEdit::setKeptFractionDigits(int n) {
    _keptFractionDigits = n;
    if (_fadeOverlay) {
        _fadeOverlay->setKeptDigits(n);
    }
}

void NumberEdit::setHasFrame(bool hasFrame) {
    _hasFrame = hasFrame;
    setProperty("frame", hasFrame);
    setAttribute(Qt::WA_StyledBackground, true);
    style()->unpolish(this);
    style()->polish(this);
    update();
}

void NumberEdit::setDefocusCallback(std::function<void()> callback) {
    _defocusCallback = std::move(callback);
}

void NumberEdit::setEvaluator(std::function<double(const QString&)> evaluator) {
    _evaluator = std::move(evaluator);
    static_cast<NumberEdit::SpinBox*>(_spinBox)->setEvaluator(evaluator);
}

double NumberEdit::value() const {
    return _spinBox->value() / _factor;
}

void NumberEdit::setValue(double val) {
    _mixedMode = false;
    _spinBox->setValue(val * _factor);
}

void NumberEdit::setValue(const Linea::mixed_property<QVariant>& val) {
    _mixedMode = val.is_mixed();
    _spinBox->setValue(val.value().toDouble() * _factor);
}

QString NumberEdit::prefix() const {
    return _spinBox->prefix();
}

void NumberEdit::setPrefix(const QString& prefix) {
    _spinBox->setPrefix(prefix);
}

QString NumberEdit::suffix() const {
    return _spinBox->suffix();
}

void NumberEdit::setSuffix(const QString& suffix) {
    _spinBox->setSuffix(suffix);
}

double NumberEdit::minimum() const {
    return _spinBox->minimum() / _factor;
}

void NumberEdit::setMinimum(double min) {
    _spinBox->setMinimum(min * _factor);
}

double NumberEdit::maximum() const {
    return _spinBox->maximum() / _factor;
}

void NumberEdit::setMaximum(double max) {
    _spinBox->setMaximum(max * _factor);
}

void NumberEdit::setRange(double min, double max) {
    _spinBox->setRange(min * _factor, max * _factor);
}

int NumberEdit::decimals() const {
    return _spinBox->decimals();
}

void NumberEdit::setDecimals(int prec) {
    _spinBox->setDecimals(prec);
}

bool NumberEdit::wrapping() const {
    return _spinBox->wrapping();
}

void NumberEdit::setWrapping(bool wrapping) {
    _spinBox->setWrapping(wrapping);
}

double NumberEdit::singleStep() const {
    return _spinBox->singleStep() / _factor;
}

void NumberEdit::setSingleStep(double step) {
    _spinBox->setSingleStep(step * _factor);
}

void NumberEdit::setFactor(double factor) {
    assert(factor > 0);
    _factor = factor;
}

void NumberEdit::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        // setValue(_preEditValue);
        _spinBox->clearFocus();
        if (_defocusCallback) {
            _defocusCallback();
        }
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

bool NumberEdit::eventFilter(QObject* watched, QEvent* event) {
    auto le = static_cast<NumberEdit::SpinBox*>(_spinBox)->getLineEdit();

    if (event->type() == QEvent::Enter
        && (watched == this || watched == _spinBox || watched == le
            || watched == _arrowDown || watched == _arrowUp)) {
        // Qt only delivers Enter to a widget the cursor really entered, and
        // the enter chain walks ancestors — so any Enter anywhere in the
        // subtree, including landing directly on an arrow, proves hover
        setHovered(true);
    } else if (event->type() == QEvent::Leave && (watched == _spinBox || watched == le)) {
        // moving onto an overlay child (arrow, indicator) fires Leave on the
        // widget beneath while the cursor stays inside the field
        setHovered(true);
    } else if (watched == this && event->type() == QEvent::Leave) {
        // leaving the NumberEdit leaves the whole subtree — but a Leave can
        // arrive without the enter/leave chain actually moving the cursor
        // (widget show/hide reshuffles), so verify underMouse before hiding
        QTimer::singleShot(150, this, [this] {
            if (!underMouse()) {
                setHovered(false);
            }
        });
    }
    else if (event->type() == QEvent::EnabledChange
             && (watched == this || watched == _spinBox)
             && (!isEnabled() || !_spinBox->isEnabled())) {
        // disabled while hovered — retract the arrows
        setHovered(false);
    }
    else if (watched == _indicator && event->type() == QEvent::Resize) {
        // the label is never laid out — it auto-sizes to sizeHint on first
        // show, which can land after the spinbox's own Resize; re-center
        // and re-evaluate the text margin once it settles at its real size
        _indicator->move(2, (_spinBox->height() - _indicator->height()) / 2);
        updateTextMargins();
    }
    else if ((watched == _spinBox || watched == this) && event->type() == QEvent::Resize
             || watched == _spinBox && event->type() == QEvent::Move) {
        // anchor the indicator overlay to the left side of the spinbox
        _indicator->move(2, (_spinBox->height() - _indicator->height()) / 2);
        // inset so the opaque arrow backdrop can't erase the parent's
        // 1px frame and rounded corners; children aren't clipped to the
        // spinbox, so they can reach past its bounds to the frame's edge
        const int arrowTop = 1 - _spinBox->y();
        const int aw = arrowWidth(width());
        _arrowDown->setGeometry(1, arrowTop, aw, height() - 2);
        _arrowUp->setGeometry(_spinBox->width() - aw - 1, arrowTop,
                              aw, height() - 2);
        updateTextMargins();
        updateIndicatorVisibility();
    }

    if (watched == static_cast<QObject*>(le)) {
        if (event->type() == QEvent::Resize) {
            // the spinbox Resize above fires before fixEditField() gives the
            // edit its real width — only now is the collision eval valid
            updateTextMargins();
        } else if (event->type() == QEvent::FocusIn) {
            _editing = true;
            _preEditValue = value();
        } else if (event->type() == QEvent::KeyPress) {
            auto keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                _editing = false;
                _spinBox->interpretText();
                Q_EMIT editingFinished();
                Q_EMIT activate();
                keyEvent->accept();
                return true;
            }
            if (keyEvent->key() == Qt::Key_Escape) {
                // setValue(_preEditValue);
                _spinBox->clearFocus();
                if (_defocusCallback) {
                    _defocusCallback();
                }
                keyEvent->accept();
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void NumberEdit::setMixedMode(bool mixed) {
    _mixedMode = mixed;
    _spinBox->setValue(_spinBox->value());
}

void NumberEdit::setPlaceholder(const QString& placeholder) {
    // _spinBox->setPlaceholderText(placeholder);
}

void NumberEdit::clearPlaceholder() {
    // _spinBox->setPlaceholderText(QString());
}

void NumberEdit::onSpinBoxValueChanged(double v) {
    // updateEdit() writes the new text with signals blocked — textChanged
    // never fires for programmatic setValue, so the margin must be
    // re-evaluated here or the label would cover freshly-arrived digits
    updateTextMargins();
    Q_EMIT valueChanged(v / _factor);
}

} // namespace Linea::UI
