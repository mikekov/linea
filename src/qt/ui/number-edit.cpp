// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * NumberEdit widget implementation.
 */

#include "number-edit.h"
#include "util/expression-evaluator.h"
#include "util/numeric/converters.h"
#include <cmath>
#include <QDoubleSpinBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QStyle>
#include <QStyleOption>

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
        , _edit(edit)
    {
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
        QRect contents = _edit->style()->subElementRect(
            QStyle::SE_LineEditContents, &option, _edit);

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
            }
            else {
                hi = mid;
            }
        }
        int text_x = lo - fm.horizontalAdvance(shown[anchor]) / 2
                        - fm.horizontalAdvance(shown.left(anchor));
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

// Internal spinbox class to access protected lineEdit() and handle formatting
class NumberEdit::SpinBox : public QDoubleSpinBox {
public:
    explicit SpinBox(NumberEdit* parentEdit, QWidget* parent = nullptr)
        : QDoubleSpinBox(parent)
        , _parentEdit(parentEdit)
    {
        setKeyboardTracking(false);
        setAlignment(Qt::AlignCenter);
    }

    void setTrimZeros(bool trim) { _trimZeros = trim; }
    bool trimZeros() const { return _trimZeros; }

    void setEvaluator(std::function<double(const QString&)> evaluator) {
        _evaluator = std::move(evaluator);
    }

    QLineEdit* getLineEdit() const { return lineEdit(); }

    QSize sizeHint() const override { return sizedHint([this] { return QDoubleSpinBox::sizeHint(); }); }
    QSize minimumSizeHint() const override { return sizedHint([this] { return QDoubleSpinBox::minimumSizeHint(); }); }

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
        return Inkscape::Util::formatTrimmed(value, decimals());
    }

    QValidator::State validate(QString& text, int& pos) const override {
        auto state = QDoubleSpinBox::validate(text, pos);
        if (state != QValidator::Invalid || !_evaluator || text.trimmed().isEmpty()) {
            return state;
        }
        // With an evaluator installed, accept expressions and unit suffixes:
        // evaluatable input is Acceptable, anything else Intermediate so it
        // can still be typed but won't commit.
        QString stripped = stripPrefixSuffix(text);
        try {
            _evaluator(stripped);
            return QValidator::Acceptable;
        }
        catch (...) {
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
            }
            catch (...) {
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
    QSize sizedHint(F&& base) const {
        _sizing = true;
        QSize hint = base();
        _sizing = false;
        return hint;
    }
};

NumberEdit::NumberEdit(QWidget* parent)
    : QWidget(parent)
    , _trimZeros(true)
{
    setObjectName("NumberEdit");
    setProperty("class", "NumberEdit"); // For stylesheet selection
    setProperty("frame", _hasFrame);
    setAttribute(Qt::WA_StyledBackground, true);

    _layout = new QHBoxLayout(this);
    _layout->setContentsMargins(0, 0, 0, 0);
    _layout->setSpacing(0);

    _indicator = new QLabel(this);
    _indicator->setObjectName("indicator");
    _indicator->setAlignment(Qt::AlignCenter);
    _indicator->setVisible(false);
    // fix label's width to lock the number box position
    _indicator->setMinimumWidth(18);
    _indicator->setMaximumWidth(30);
    _layout->addWidget(_indicator);

    _spinBox = new NumberEdit::SpinBox(this, this);
    _spinBox->setObjectName("spinBox");
    _spinBox->setFrame(false);
    _spinBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    _spinBox->setMinimumWidth(18);
    _layout->addWidget(_spinBox, 1);

    _spinBox->installEventFilter(this);
    if (auto le = static_cast<NumberEdit::SpinBox*>(_spinBox)->getLineEdit()) {
        le->installEventFilter(this);
        _fadeOverlay = new FadeOverlay(le);
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

void NumberEdit::setLabel(const QString& text) {
    _indicator->clear();
    _indicator->setText(text);
    // _indicator->setPixmap(QPixmap());
    _indicator->setVisible(!text.isEmpty());
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
    _indicator->setVisible(!icon.isNull());
}

void NumberEdit::clearIndicator() {
    _indicator->setText(QString());
    _indicator->setPixmap(QPixmap());
    _indicator->setVisible(false);
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
    if (watched == static_cast<QObject*>(le)) {
        if (event->type() == QEvent::FocusIn) {
            _editing = true;
            _preEditValue = value();
        }
        else if (event->type() == QEvent::KeyPress) {
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
    Q_EMIT valueChanged(v / _factor);
}

} // namespace Linea::UI
