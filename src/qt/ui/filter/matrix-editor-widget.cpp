// SPDX-License-Identifier: GPL-2.0-or-later
/** @file Qt editor for SVG filter matrix-valued attributes. */

#include "matrix-editor-widget.h"

#include <QGridLayout>
#include <QRegularExpression>
#include <QVBoxLayout>

#include "number-edit.h"
#include "object/filters/sp-filter-primitive.h"
#include "spin-scale.h"
#include "svg/css-ostringstream.h"
#include "xml/repr.h"

namespace Linea::UI {

MatrixEditorWidget::MatrixEditorWidget(QWidget* parent)
    : QWidget(parent)
    , _gridWidget(new QWidget(this))
    , _grid(new QGridLayout(_gridWidget)) {
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    _grid->setContentsMargins(0, 0, 0, 0);
    _grid->setHorizontalSpacing(2);
    _grid->setVerticalSpacing(2);
    layout->addWidget(_gridWidget);
}

void MatrixEditorWidget::setPrimitive(SPFilterPrimitive* primitive, const QString& attribute, int rows, int columns) {
    if (!primitive || !primitive->getRepr() || attribute.isEmpty() || rows < 1 || columns < 1) {
        _primitive = nullptr;
        _attribute.clear();
        _rows = _columns = 0;
        rebuild();
        return;
    }
    _primitive = primitive;
    _attribute = attribute;
    _rows = rows;
    _columns = columns;
    rebuild();
}

void MatrixEditorWidget::rebuild() {
    _loading = true;
    while (auto item = _grid->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    _edits.clear();
    if (!_primitive || !_primitive->getRepr() || _rows < 1 || _columns < 1) {
        _loading = false;
        return;
    }

    const auto raw = _primitive->getRepr()->attribute(_attribute.toUtf8().constData());
    const auto parts = QString::fromUtf8(raw ?: "").split(QRegularExpression(QStringLiteral("[\\s,]+")), Qt::SkipEmptyParts);
    const int count = _rows * _columns;
    _edits.reserve(count);
    for (int index = 0; index < count; ++index) {
        auto edit = new SpinScale(_gridWidget);
        edit->setRange(-100000.0, 100000.0);
        edit->setDecimals(4);
        edit->setSingleStep(0.01);
        edit->numberEdit()->setLabel(QString::number(index + 1));
        bool valid = false;
        const auto value = index < parts.size() ? parts.at(index).toDouble(&valid) : 0.0;
        // SVG's omitted color-matrix value is the identity matrix; the
        // omitted convolution kernel is conventionally a center impulse.
        double initial = valid ? value : 0.0;
        if (!raw) {
            if (_attribute == QStringLiteral("values") && index / _columns == index % _columns && index / _columns < 4)
                initial = 1.0;
            if (_attribute == QStringLiteral("kernelMatrix") && index == count / 2)
                initial = 1.0;
        }
        edit->setValue(initial);
        _edits.push_back(edit);
        _grid->addWidget(edit, index / _columns, index % _columns);
        connect(edit, &SpinScale::valueChanged, this, [this](double) {
            if (!_loading) writeValues();
        });
    }
    _loading = false;
}

void MatrixEditorWidget::writeValues() {
    if (_loading || !_primitive || !_primitive->getRepr() || _edits.isEmpty()) return;

    Inkscape::CSSOStringStream stream;
    for (int index = 0; index < _edits.size(); ++index) {
        if (index) stream << ' ';
        stream << _edits.at(index)->value();
    }
    const auto value = QString::fromStdString(stream.str());
    const auto key = _attribute.toUtf8();
    const auto data = value.toUtf8();
    _primitive->setAttributeOrRemoveIfEmpty(key.constData(), data.constData());
    _primitive->requestModified(SP_OBJECT_MODIFIED_FLAG);
    Q_EMIT primitiveChanged(_primitive, _attribute, value);
}

} // namespace Linea::UI
