// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * NumberEdit widget implementation.
 */

#include "number-edit.h"
#include "util/numeric/converters.h"
#include <QDoubleSpinBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QStyle>

namespace Linea::UI {

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

    double valueFromText(const QString& text) const override {
        if (_evaluator) {
            QString stripped = stripPrefixSuffix(text);
            bool numberOk = false;
            stripped.toDouble(&numberOk);
            if (!numberOk) {
                try {
                    return _evaluator(stripped);
                }
                catch (...) {
                    // Fall through to default parsing
                }
            }
        }
        return QDoubleSpinBox::valueFromText(text);
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
    }

    // Use old-style macros to avoid assertObjectType runtime check across library boundaries
    connect(_spinBox, SIGNAL(valueChanged(double)), this, SLOT(onSpinBoxValueChanged(double)));
    connect(_spinBox, SIGNAL(editingFinished()), this, SIGNAL(editingFinished()));
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
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                _editing = false;
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
