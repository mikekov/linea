// SPDX-License-Identifier: GPL-2.0-or-later
/** @file Qt editor for one feComponentTransfer primitive. */

#include "component-transfer-settings-widget.h"

#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSignalBlocker>

#include "filter-enums.h"
#include "gc-anchored.h"
#include "number-edit.h"
#include "object/filters/componenttransfer-funcnode.h"
#include "object/filters/componenttransfer.h"
#include "object/filters/sp-filter-primitive.h"
#include "spin-scale.h"
#include "svg-attribute-format.h"
#include "xml/simple-node.h"

namespace Linea::UI {
namespace {

const char* channelElement(SPFeFuncNode::Channel channel) {
    switch (channel) {
        case SPFeFuncNode::R:
            return "svg:feFuncR";
        case SPFeFuncNode::G:
            return "svg:feFuncG";
        case SPFeFuncNode::B:
            return "svg:feFuncB";
        case SPFeFuncNode::A:
            return "svg:feFuncA";
    }
    return nullptr;
}

SPFeFuncNode::Channel toChannel(int index) {
    return static_cast<SPFeFuncNode::Channel>(index);
}

} // namespace

ComponentTransferSettingsWidget::ComponentTransferSettingsWidget(QWidget* parent)
    : QWidget(parent)
    , _form(new QGridLayout(this))
    , _channelCombo(new QComboBox(this))
    , _typeCombo(new QComboBox(this))
    , _slope(new SpinScale(this))
    , _intercept(new SpinScale(this))
    , _amplitude(new SpinScale(this))
    , _exponent(new SpinScale(this))
    , _offset(new SpinScale(this))
    , _tableValues(new QLineEdit(this)) {
    _form->setContentsMargins(0, 0, 0, 0);
    _form->setHorizontalSpacing(4);
    _form->setVerticalSpacing(4);
    _form->setColumnMinimumWidth(0, 120);
    _form->setColumnStretch(1, 1);

    _channelCombo->addItems({tr("Red (R)"), tr("Green (G)"), tr("Blue (B)"), tr("Alpha (A)")});
    for (unsigned int i = 0; i < ComponentTransferTypeConverter._length; ++i) {
        const auto& data = ComponentTransferTypeConverter.data(i);
        _typeCombo->addItem(tr(data.label.c_str()), static_cast<int>(data.id));
    }

    auto configure = [](SpinScale* edit, double minimum, double maximum, double value) {
        edit->setRange(minimum, maximum);
        edit->setDecimals(2);
        edit->setSingleStep(0.01);
        edit->setValue(value);
    };
    configure(_slope, -10, 10, 1);
    configure(_intercept, -10, 10, 0);
    configure(_amplitude, 0, 10, 1);
    configure(_exponent, 0, 10, 1);
    configure(_offset, -10, 10, 0);

    int row = 0;
    _form->addWidget(new QLabel(tr("Channel:"), this), row, 0);
    _form->addWidget(_channelCombo, row++, 1);
    _form->addWidget(new QLabel(tr("Type:"), this), row, 0);
    _form->addWidget(_typeCombo, row++, 1);
    auto add = [this, &row](const QString& label, QWidget* widget) {
        _form->addWidget(new QLabel(label + QLatin1Char(':'), this), row, 0);
        _form->addWidget(widget, row++, 1);
    };
    add(tr("Slope"), _slope);
    add(tr("Intercept"), _intercept);
    add(tr("Amplitude"), _amplitude);
    add(tr("Exponent"), _exponent);
    add(tr("Offset"), _offset);
    add(tr("Values"), _tableValues);

    connect(_channelCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &ComponentTransferSettingsWidget::setChannel);
    connect(_typeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (_loading.pending() || index < 0) return;
        ensureFuncNode();
        if (!_primitive || !_typeCombo->itemData(index).isValid()) return;
        const auto type =
            static_cast<Inkscape::Filters::FilterComponentTransferType>(_typeCombo->itemData(index).toInt());
        writeAttribute("type", QString::fromUtf8(ComponentTransferTypeConverter.get_key(type).c_str()));
        updateSensitivity();
    });

    auto connectNumber = [this](SpinScale* edit, const char* attribute) {
        connect(edit, &SpinScale::valueChanged, this, [this, edit, attribute](double) {
            if (_loading.pending()) return;
            writeAttribute(attribute, Filter::format_number(edit->value()));
        });
    };
    connectNumber(_slope, "slope");
    connectNumber(_intercept, "intercept");
    connectNumber(_amplitude, "amplitude");
    connectNumber(_exponent, "exponent");
    connectNumber(_offset, "offset");
    connect(_tableValues, &QLineEdit::editingFinished, this, [this] {
        if (!_loading.pending()) writeAttribute("tableValues", _tableValues->text());
    });
    updateSensitivity();
}

void ComponentTransferSettingsWidget::setPrimitive(SPFilterPrimitive* primitive) {
    if (_writing.pending()) return;
    if (_primitive == primitive) {
        load();
        return;
    }
    _primitive = primitive;
    load();
}

void ComponentTransferSettingsWidget::setChannel(int channel) {
    if (channel < 0 || channel > 3) return;
    _channel = channel;
    if (_channelCombo->currentIndex() != channel) {
        QSignalBlocker blocker(_channelCombo);
        _channelCombo->setCurrentIndex(channel);
    }
    load();
}

void ComponentTransferSettingsWidget::ensureFuncNode() {
    if (!_primitive || !is<SPFeComponentTransfer>(_primitive)) return;
    auto ct = cast<SPFeComponentTransfer>(_primitive);
    for (auto& child : ct->children) {
        auto node = cast<SPFeFuncNode>(&child);
        if (node && node->channel == toChannel(_channel)) return;
    }
    if (!_primitive->document || !_primitive->document->getReprDoc()) return;
    auto name = channelElement(toChannel(_channel));
    if (!name) return;
    auto repr = _primitive->document->getReprDoc()->createElement(name);
    repr->setAttribute("type", "identity");
    _primitive->getRepr()->appendChild(repr);
    Inkscape::GC::release(repr);
    _primitive->requestModified(SP_OBJECT_MODIFIED_FLAG);
}

void ComponentTransferSettingsWidget::load() {
    auto loadingBlock = _loading.block();
    ensureFuncNode();
    SPFeFuncNode* node = nullptr;
    if (_primitive && is<SPFeComponentTransfer>(_primitive)) {
        auto ct = cast<SPFeComponentTransfer>(_primitive);
        for (auto& child : ct->children) {
            auto candidate = cast<SPFeFuncNode>(&child);
            if (candidate && candidate->channel == toChannel(_channel)) {
                node = candidate;
                break;
            }
        }
    }
    if (!node || !node->getRepr()) {
        updateSensitivity();
        return;
    }
    const auto type = node->getRepr()->attribute("type");
    int typeIndex = _typeCombo->findData(
        static_cast<int>(ComponentTransferTypeConverter.get_id_from_key(type ? type : "identity")));
    if (typeIndex < 0)
        typeIndex = _typeCombo->findData(static_cast<int>(Inkscape::Filters::COMPONENTTRANSFER_TYPE_IDENTITY));
    {
        QSignalBlocker blocker(_typeCombo);
        _typeCombo->setCurrentIndex(typeIndex);
    }
    auto number = [node](const char* name, double fallback) {
        const auto value = node->getRepr()->attribute(name);
        if (!value) return fallback;
        bool ok = false;
        const auto result = QString::fromUtf8(value).toDouble(&ok);
        return ok ? result : fallback;
    };
    _slope->setValue(number("slope", 1));
    _intercept->setValue(number("intercept", 0));
    _amplitude->setValue(number("amplitude", 1));
    _exponent->setValue(number("exponent", 1));
    _offset->setValue(number("offset", 0));
    _tableValues->setText(QString::fromUtf8(node->getRepr()->attribute("tableValues") ?: ""));
    updateSensitivity();
}

void ComponentTransferSettingsWidget::writeAttribute(const char* attribute, const QString& value) {
    if (_loading.pending() || _writing.pending() || !_primitive) return;
    auto writingBlock = _writing.block();
    ensureFuncNode();
    SPFeFuncNode* node = nullptr;
    if (is<SPFeComponentTransfer>(_primitive)) {
        auto ct = cast<SPFeComponentTransfer>(_primitive);
        for (auto& child : ct->children) {
            auto candidate = cast<SPFeFuncNode>(&child);
            if (candidate && candidate->channel == toChannel(_channel)) {
                node = candidate;
                break;
            }
        }
    }
    if (node) {
        node->setAttributeOrRemoveIfEmpty(attribute, value.toUtf8().constData());
        Q_EMIT primitiveChanged(_primitive, QString::fromLatin1(attribute), value);
    }
}

void ComponentTransferSettingsWidget::updateSensitivity() {
    const auto type = _typeCombo ? _typeCombo->currentData().toInt() : -1;
    const auto linear = type == Inkscape::Filters::COMPONENTTRANSFER_TYPE_LINEAR;
    const auto gamma = type == Inkscape::Filters::COMPONENTTRANSFER_TYPE_GAMMA;
    const auto table = type == Inkscape::Filters::COMPONENTTRANSFER_TYPE_TABLE ||
                       type == Inkscape::Filters::COMPONENTTRANSFER_TYPE_DISCRETE;
    auto setRowVisible = [this](int row, bool visible) {
        if (auto item = _form->itemAtPosition(row, 0); item && item->widget()) item->widget()->setVisible(visible);
        if (auto item = _form->itemAtPosition(row, 1); item && item->widget()) item->widget()->setVisible(visible);
    };
    setRowVisible(2, linear); // slope
    setRowVisible(3, linear); // intercept
    setRowVisible(4, gamma);  // amplitude
    setRowVisible(5, gamma);  // exponent
    setRowVisible(6, gamma);  // offset
    setRowVisible(7, table);  // tableValues
}

} // namespace Linea::UI
