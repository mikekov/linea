// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Qt editor for SVG filter primitive attributes.
 */

#include "primitive-settings-widget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QObject>
#include <QPushButton>
#include <QRegularExpression>
#include <QStringList>
#include <algorithm>
#include <array>
#include <initializer_list>
#include <map>
#include <memory>
#include <vector>

#include "desktop.h"
#include "display/nr-filter-blend.h"
#include "filter-enums.h"
#include "matrix-editor-widget.h"
#include "number-edit.h"
#include "object/filters/sp-filter-primitive.h"
#include "spin-scale.h"
#include "selection.h"
#include "svg-attribute-format.h"
#include "util/enums.h"
#include "xml/repr.h"

namespace Linea::UI {

namespace {

enum class ControlType { Text, Number, DualNumber, Choice, Boolean, File, Subregion };

struct AttributeSpec {
    const char* name;
    ControlType type;
    double minimum = -100000.0;
    double maximum = 100000.0;
    int decimals = 2;
    double step = 0.01;
    const char* suffix = "";
    QStringList choices;
    QStringList values;
};

AttributeSpec number(const char* name, double minimum, double maximum, int decimals = 2, double step = 0.01,
                     const char* suffix = "") {
    return {name, ControlType::Number, minimum, maximum, decimals, step, suffix, {}};
}

AttributeSpec dualNumber(const char* name, double minimum, double maximum, int decimals = 2, double step = 0.01,
                         const char* suffix = "") {
    return {name, ControlType::DualNumber, minimum, maximum, decimals, step, suffix, {}};
}

AttributeSpec text(const char* name) {
    return {name, ControlType::Text};
}

AttributeSpec boolean(const char* name) {
    return {name, ControlType::Boolean};
}

AttributeSpec file(const char* name) {
    return {name, ControlType::File};
}

AttributeSpec subregion() {
    return {"subregion", ControlType::Subregion};
}

template <typename E>
AttributeSpec choice(const char* name, const Inkscape::Util::EnumDataConverter<E>& converter) {
    QStringList labels;
    QStringList values;
    for (unsigned int index = 0; index < converter._length; ++index) {
        const auto& data = converter.data(index);
        labels.append(QObject::tr(data.label.c_str()));
        values.append(QString::fromUtf8(data.key.c_str()));
    }
    return {name, ControlType::Choice, 0, 0, 0, 0, "", labels, values};
}

std::vector<AttributeSpec> settingsFor(const QString& element) {
    std::vector<AttributeSpec> settings;
    if (element == QStringLiteral("feColorMatrix")) {
        settings = {choice("type", ColorMatrixTypeConverter)};
    } else if (element == QStringLiteral("feFlood")) {
        settings = {text("flood-color"), number("flood-opacity", 0, 1, 3, 0.01)};
    } else if (element == QStringLiteral("feOffset")) {
        settings = {boolean("preserveAlpha"), number("dx", -100000, 100000, 2, 0.1, " px"),
                    number("dy", -100000, 100000, 2, 0.1, " px")};
    } else if (element == QStringLiteral("feMorphology")) {
        settings = {choice("operator", MorphologyOperatorConverter), dualNumber("radius", 0, 100000, 2, 0.1)};
    } else if (element == QStringLiteral("feComposite")) {
        settings = {choice("operator", CompositeOperatorConverter), number("k1", -100000, 100000),
                    number("k2", -100000, 100000), number("k3", -100000, 100000), number("k4", -100000, 100000)};
    } else if (element == QStringLiteral("feConvolveMatrix")) {
        settings = {dualNumber("order", 1, 100, 0, 1),
                    text("targetX"),
                    text("targetY"),
                    number("divisor", -100000, 100000),
                    number("bias", -100000, 100000),
                    choice("edgeMode", ConvolveMatrixEdgeModeConverter),
                    boolean("preserveAlpha")};
    } else if (element == QStringLiteral("feBlend")) {
        settings = {choice("mode", SPBlendModeConverter)};
    } else if (element == QStringLiteral("feDisplacementMap")) {
        settings = {number("scale", -100000, 100000), choice("xChannelSelector", DisplacementMapChannelConverter),
                    choice("yChannelSelector", DisplacementMapChannelConverter)};
    } else if (element == QStringLiteral("feTurbulence")) {
        settings = {number("seed", -100000, 100000, 0, 1), number("numOctaves", 0, 32, 0, 1),
                    dualNumber("baseFrequency", 0, 1, 3, 0.001), choice("type", TurbulenceTypeConverter)};
    } else if (element == QStringLiteral("feDiffuseLighting")) {
        settings = {text("lighting-color"), number("surfaceScale", -5, 5, 3, 0.001),
                    number("diffuseConstant", 0, 5, 2, 0.01)};
    } else if (element == QStringLiteral("feGaussianBlur")) {
        settings = {dualNumber("stdDeviation", 0, 1000, 2, 0.1)};
    } else if (element == QStringLiteral("feImage")) {
        settings = {file("xlink:href"), subregion()};
    } else if (element == QStringLiteral("feTile")) {
        settings = {subregion()};
    } else if (element == QStringLiteral("feSpecularLighting")) {
        settings = {text("lighting-color"), number("surfaceScale", -5, 5, 2, 0.01),
                    number("specularConstant", 0, 5, 2, 0.01), number("specularExponent", 1, 50, 1, 0.01)};
    }

    return settings;
}

QString labelFor(const QString& attribute) {
    QString label = attribute;
    label.replace(QRegularExpression(QStringLiteral("([a-z])([A-Z])")), QStringLiteral("\\1 \\2"));
    label.replace('-', ' ');
    return label.left(1).toUpper() + label.mid(1);
}

QString defaultValueFor(const QString& element, const QString& attribute) {
    if (attribute == QStringLiteral("flood-color")) return QStringLiteral("#000000");
    if (attribute == QStringLiteral("lighting-color")) return QStringLiteral("#ffffff");
    if (attribute == QStringLiteral("flood-opacity")) return QStringLiteral("1");
    if (attribute == QStringLiteral("operator")) {
        return element == QStringLiteral("feMorphology") ? QStringLiteral("erode") : QStringLiteral("over");
    }
    if (attribute == QStringLiteral("mode")) return QStringLiteral("normal");
    if (attribute == QStringLiteral("stdDeviation")) return QStringLiteral("0");
    if (attribute == QStringLiteral("order")) return QStringLiteral("3 3");
    if (attribute == QStringLiteral("divisor")) return QStringLiteral("1");
    if (attribute == QStringLiteral("edgeMode")) return QStringLiteral("none");
    if (attribute == QStringLiteral("type")) return element == QStringLiteral("feTurbulence") ? QStringLiteral("turbulence") : QStringLiteral("matrix");
    if (attribute == QStringLiteral("xChannelSelector") || attribute == QStringLiteral("yChannelSelector")) return QStringLiteral("A");
    if (attribute == QStringLiteral("numOctaves")) return QStringLiteral("1");
    if (attribute == QStringLiteral("baseFrequency")) return QStringLiteral("0");
    if (attribute == QStringLiteral("diffuseConstant") || attribute == QStringLiteral("specularConstant")) return QStringLiteral("1");
    if (attribute == QStringLiteral("specularExponent")) return QStringLiteral("1");
    return QStringLiteral("0");
}

QString tooltipFor(const QString& element, const QString& attribute) {
    if (attribute == QStringLiteral("flood-color")) return QObject::tr("Color used to fill the filter region.");
    if (attribute == QStringLiteral("flood-opacity")) return QObject::tr("Opacity of the flood color.");
    if (attribute == QStringLiteral("stdDeviation")) return QObject::tr("Standard deviation of the Gaussian blur.");
    if (attribute == QStringLiteral("surfaceScale")) return QObject::tr("Amplifies heights from the input alpha channel.");
    if (attribute == QStringLiteral("diffuseConstant")) return QObject::tr("Constant used by the Phong lighting model.");
    if (attribute == QStringLiteral("specularConstant")) return QObject::tr("Constant used by the Phong lighting model.");
    if (attribute == QStringLiteral("specularExponent")) return QObject::tr("Exponent controlling the specular highlight.");
    if (attribute == QStringLiteral("scale")) return QObject::tr("Intensity of the displacement effect.");
    if (attribute == QStringLiteral("seed")) return QObject::tr("Starting value for the turbulence random generator.");
    if (attribute == QStringLiteral("numOctaves")) return QObject::tr("Number of noise layers used by the turbulence.");
    if (attribute == QStringLiteral("baseFrequency")) return QObject::tr("Base frequency of the turbulence.");
    if (attribute == QStringLiteral("operator")) {
        return element == QStringLiteral("feMorphology") ? QObject::tr("Erode or dilate the input.")
                                                            : QObject::tr("Composite operation applied to the inputs.");
    }
    return QObject::tr("Edit the %1 attribute.").arg(labelFor(attribute));
}

} // namespace

PrimitiveSettingsWidget::PrimitiveSettingsWidget(QWidget* parent)
    : QWidget(parent)
    , _form(new QGridLayout(this)) {
    _form->setContentsMargins(0, 0, 0, 0);
    _form->setHorizontalSpacing(4);
    _form->setVerticalSpacing(4);
    _form->setColumnMinimumWidth(0, 220);
    _form->setColumnStretch(1, 1);
}

void PrimitiveSettingsWidget::setDesktop(SPDesktop* desktop) {
    _desktop = desktop;
}

void PrimitiveSettingsWidget::setPrimitive(SPFilterPrimitive* primitive) {
    if (_primitive == primitive) return;
    _primitive = primitive;
    clearForm();
    addSettings();
}

void PrimitiveSettingsWidget::clearForm() {
    while (auto item = _form->takeAt(0)) {
        delete item->widget();
        delete item;
    }
}

void PrimitiveSettingsWidget::addSettings() {
    if (!_primitive || !_primitive->getRepr()) return;

    auto element = QString::fromUtf8(_primitive->getRepr()->name());
    if (element.startsWith(QStringLiteral("svg:"))) {
        element.remove(0, 4);
    }
    auto settings = settingsFor(element);
    auto loadingBlock = _loading.block();
    std::map<QString, QWidget*> editors;
    int row = 0;
    auto add_row = [this, &row](const QString& label, QWidget* editor) {
        auto label_widget = new QLabel(label + QStringLiteral(":"), this);
        label_widget->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        _form->addWidget(label_widget, row, 0);
        _form->addWidget(editor, row, 1);
        ++row;
    };
    if (settings.empty()) {
        auto message = new QLabel(tr("This SVG filter effect does not require any parameters."), this);
        message->setWordWrap(true);
        _form->addWidget(message, row++, 0, 1, 2);
        return;
    }

    for (const auto& spec : settings) {
        const auto attribute = QString::fromLatin1(spec.name);
        const auto value = QString::fromUtf8(_primitive->getRepr()->attribute(spec.name) ?: "");
        const auto initialValue = value.isEmpty() ? defaultValueFor(element, attribute) : value;
        QWidget* editor = nullptr;

        if (spec.type == ControlType::Number) {
            auto spin = new SpinScale(this);
            spin->setRange(spec.minimum, spec.maximum);
            spin->setDecimals(spec.decimals);
            spin->setSingleStep(spec.step);
            spin->setSuffix(QString::fromLatin1(spec.suffix));
            bool valid = false;
            const auto numeric = initialValue.toDouble(&valid);
            spin->setValue(valid ? numeric : 0.0);
            connect(spin, &SpinScale::valueChanged, this,
                    [this, attribute](double number) { setAttribute(attribute, Filter::format_number(number)); });
            editor = spin;
        } else if (spec.type == ControlType::Subregion) {
            auto container = new QWidget(this);
            auto layout = new QVBoxLayout(container);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(4);
            auto check = new QCheckBox(tr("Define subregion"), container);
            auto valuesGrid = new QGridLayout();
            valuesGrid->setContentsMargins(0, 0, 0, 0);
            valuesGrid->setHorizontalSpacing(4);
            valuesGrid->setVerticalSpacing(4);
            const std::array<const char*, 4> names{"x", "y", "width", "height"};
            const std::array<double, 4> defaults{0.0, 0.0, 100.0, 100.0};
            std::array<SpinScale*, 4> edits{};
            bool active = false;
            for (int index = 0; index < 4; ++index) {
                const auto raw = _primitive->getRepr()->attribute(names[index]);
                if (raw) active = true;
                auto edit = new SpinScale(container);
                edit->numberEdit()->setLabel(QString::fromLatin1(names[index]).toUpper());
                edit->setRange(index < 2 ? -10000000.0 : 0.0, 10000000.0);
                edit->setDecimals(3);
                edit->setSingleStep(1.0);
                bool valid = false;
                const auto value = raw ? QString::fromUtf8(raw).toDouble(&valid) : defaults[index];
                edit->setValue(valid ? value : defaults[index]);
                edits[index] = edit;
                valuesGrid->addWidget(edit, index / 2, index % 2);
                connect(edit, &SpinScale::valueChanged, this, [this, edit, names, index](double) {
                    if (!_loading.pending()) setAttribute(names[index], Filter::format_number(edit->value()));
                });
            }
            check->setChecked(active);
            valuesGrid->setEnabled(active);
            connect(check, &QCheckBox::toggled, this, [this, valuesGrid, edits, names](bool enabled) {
                valuesGrid->setEnabled(enabled);
                if (!enabled) {
                    for (auto name : names) setAttribute(name, QString());
                } else {
                    for (int index = 0; index < 4; ++index)
                        setAttribute(names[index], Filter::format_number(edits[index]->value()));
                }
            });
            layout->addWidget(check);
            layout->addLayout(valuesGrid);
            editor = container;
        } else if (spec.type == ControlType::File) {
            auto container = new QWidget(this);
            auto layout = new QHBoxLayout(container);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(4);
            auto edit = new QLineEdit(initialValue, container);
            auto fileButton = new QPushButton(tr("Browse..."), container);
            auto elementButton = new QPushButton(tr("SVG Element"), container);
            layout->addWidget(edit);
            layout->addWidget(fileButton);
            layout->addWidget(elementButton);
            connect(edit, &QLineEdit::editingFinished, this,
                    [this, edit, attribute] { setAttribute(attribute, edit->text()); });
            connect(fileButton, &QPushButton::clicked, this, [this, edit] {
                const auto fileName = QFileDialog::getOpenFileName(this, tr("Select an image to be used as input."));
                if (!fileName.isEmpty()) {
                    edit->setText(fileName);
                    setAttribute(QStringLiteral("xlink:href"), fileName);
                }
            });
            connect(elementButton, &QPushButton::clicked, this, [this, edit] {
                if (!_desktop) return;
                auto selection = _desktop->getSelection();
                if (!selection || selection->isEmpty()) return;
                auto node = selection->xmlNodes().front();
                if (!node || !node->matchAttributeName("id")) return;
                const auto value = QStringLiteral("#") + QString::fromUtf8(node->attribute("id"));
                edit->setText(value);
                setAttribute(QStringLiteral("xlink:href"), value);
            });
            editor = container;
        } else if (spec.type == ControlType::Boolean) {
            auto check = new QCheckBox(this);
            check->setChecked(value == QStringLiteral("true"));
            connect(check, &QCheckBox::toggled, this, [this, attribute](bool checked) {
                setAttribute(attribute, checked ? QStringLiteral("true") : QStringLiteral("false"));
            });
            editor = check;
        } else if (spec.type == ControlType::DualNumber) {
            // SVG dual-number attributes (e.g. stdDeviation) are stored as
            // "x" when both components are equal, or "x y" when they differ.
            auto container = new QWidget(this);
            auto box = new QHBoxLayout(container);
            box->setContentsMargins(0, 0, 0, 0);
            box->setSpacing(2);

            const auto parts = initialValue.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            bool ok_x = false, ok_y = false;
            const double x = parts.size() > 0 ? parts.at(0).toDouble(&ok_x) : 0.0;
            const double y = parts.size() > 1 ? parts.at(1).toDouble(&ok_y) : (ok_x ? x : 0.0);

            auto xEdit = new SpinScale(container);
            xEdit->numberEdit()->setLabel("X");
            auto yEdit = new SpinScale(container);
            yEdit->numberEdit()->setLabel("Y");
            for (auto edit : {xEdit, yEdit}) {
                edit->setRange(spec.minimum, spec.maximum);
                edit->setDecimals(spec.decimals);
                edit->setSingleStep(spec.step);
                edit->setSuffix(QString::fromLatin1(spec.suffix));
            }
            xEdit->setValue(x);
            yEdit->setValue(y);

            auto linkButton = new QPushButton(container);
            auto linked = std::make_shared<bool>(x == y);
            auto updateLinkIcon = [linkButton](bool isLinked) {
                linkButton->setIcon(QIcon(isLinked ? QStringLiteral(":/icons/entries-linked")
                                                   : QStringLiteral(":/icons/entries-unlinked")));
            };
            updateLinkIcon(*linked);
            yEdit->setEnabled(!*linked);

            auto writeValue = [this, attribute, xEdit, yEdit]() {
                const double xv = xEdit->value();
                const double yv = yEdit->value();
                QString formatted;
                if (xv == yv) {
                    formatted = Filter::format_number(xv);
                } else {
                    formatted = Filter::format_number(xv) + QLatin1Char(' ') + Filter::format_number(yv);
                }
                setAttribute(attribute, formatted);
            };

            connect(xEdit, &SpinScale::valueChanged, this, [this, xEdit, yEdit, linked, writeValue](double) {
                if (_loading.pending()) return;
                if (*linked) {
                    QSignalBlocker block(yEdit);
                    yEdit->setValue(xEdit->value());
                }
                writeValue();
            });
            connect(yEdit, &SpinScale::valueChanged, this, [this, writeValue](double) {
                if (_loading.pending()) return;
                writeValue();
            });
            connect(linkButton, &QPushButton::clicked, this, [xEdit, yEdit, linked, updateLinkIcon, writeValue]() {
                *linked = !*linked;
                updateLinkIcon(*linked);
                yEdit->setEnabled(!*linked);
                if (*linked) {
                    QSignalBlocker block(yEdit);
                    yEdit->setValue(xEdit->value());
                    writeValue();
                }
            });

            box->addWidget(xEdit);
            box->addWidget(linkButton);
            box->addWidget(yEdit);
            editor = container;
        } else if (spec.type == ControlType::Choice) {
            auto combo = new QComboBox(this);
            for (int index = 0; index < spec.choices.size(); ++index) {
                const auto enumValue = spec.values.at(index);
                if (enumValue == QStringLiteral("-")) {
                    combo->insertSeparator(combo->count());
                } else {
                    combo->addItem(spec.choices.at(index), enumValue);
                }
            }
            auto index = combo->findData(initialValue);
            if (index < 0 && !value.isEmpty()) {
                combo->insertItem(0, value, value);
                index = 0;
            }
            if (index >= 0) combo->setCurrentIndex(index);
            connect(combo, &QComboBox::currentIndexChanged, this,
                    [this, combo, attribute](int) { setAttribute(attribute, combo->currentData().toString()); });
            editor = combo;
        } else {
            auto edit = new QLineEdit(initialValue, this);
            connect(edit, &QLineEdit::editingFinished, this,
                    [this, edit, attribute] { setAttribute(attribute, edit->text()); });
            editor = edit;
        }
        editor->setToolTip(tooltipFor(element, attribute));
        add_row(labelFor(attribute), editor);
        editors.emplace(attribute, editor);
    }

    if (element == QStringLiteral("feColorMatrix")) {
        auto matrix = new MatrixEditorWidget(this);
        matrix->setPrimitive(_primitive, QStringLiteral("values"), 4, 5);
        connect(matrix, &MatrixEditorWidget::primitiveChanged, this,
                [this](SPFilterPrimitive* primitive, const QString& attribute, const QString& value) {
                    if (!_writing.pending()) Q_EMIT primitiveChanged(primitive, attribute, value);
                });
        add_row(tr("Values"), matrix);
        if (auto typeEditor = qobject_cast<QComboBox*>(editors[QStringLiteral("type")])) {
            auto updateMatrixVisibility = [matrix, typeEditor] {
                matrix->setVisible(typeEditor->currentData().toString() == QStringLiteral("matrix"));
            };
            connect(typeEditor, qOverload<int>(&QComboBox::currentIndexChanged), this,
                    [updateMatrixVisibility](int) { updateMatrixVisibility(); });
            updateMatrixVisibility();

            auto shortcut = new SpinScale(this);
            shortcut->setRange(-360.0, 360.0);
            shortcut->setDecimals(2);
            shortcut->setSingleStep(1.0);
            bool valid = false;
            const auto shortcutValue = QString::fromUtf8(_primitive->getRepr()->attribute("values") ?: "0")
                                           .toDouble(&valid);
            shortcut->setValue(valid ? shortcutValue : 0.0);
            shortcut->setToolTip(tr("Value used by the selected color-matrix shortcut mode."));
            add_row(tr("Value"), shortcut);
            auto updateShortcutVisibility = [shortcut, typeEditor] {
                const auto type = typeEditor->currentData().toString();
                shortcut->setVisible(type == QStringLiteral("saturate") || type == QStringLiteral("hueRotate"));
            };
            connect(typeEditor, qOverload<int>(&QComboBox::currentIndexChanged), this,
                    [updateShortcutVisibility](int) { updateShortcutVisibility(); });
            connect(shortcut, &SpinScale::valueChanged, this, [this, shortcut](double) {
                if (!_loading.pending()) setAttribute(QStringLiteral("values"), Filter::format_number(shortcut->value()));
            });
            updateShortcutVisibility();
        }
    } else if (element == QStringLiteral("feConvolveMatrix")) {
        int columns = 3;
        int rows = 3;
        const auto order = QString::fromUtf8(_primitive->getRepr()->attribute("order") ?: "3 3")
                               .split(QRegularExpression(QStringLiteral("[\\s,]+")), Qt::SkipEmptyParts);
        if (!order.isEmpty()) columns = std::max(1, order.at(0).toInt());
        if (order.size() > 1) rows = std::max(1, order.at(1).toInt());
        auto matrix = new MatrixEditorWidget(this);
        matrix->setPrimitive(_primitive, QStringLiteral("kernelMatrix"), rows, columns);
        connect(matrix, &MatrixEditorWidget::primitiveChanged, this,
                [this](SPFilterPrimitive* primitive, const QString& attribute, const QString& value) {
                    if (!_writing.pending()) Q_EMIT primitiveChanged(primitive, attribute, value);
                });
        add_row(tr("Kernel matrix"), matrix);
    }

    if (element == QStringLiteral("feComposite")) {
        auto operatorEditor = qobject_cast<QComboBox*>(editors[QStringLiteral("operator")]);
        std::array<QWidget*, 4> coefficients{
            editors[QStringLiteral("k1")], editors[QStringLiteral("k2")],
            editors[QStringLiteral("k3")], editors[QStringLiteral("k4")]};
        auto updateCoefficientSensitivity = [operatorEditor, coefficients] {
            const bool arithmetic = operatorEditor && operatorEditor->currentData().toString() == QStringLiteral("arithmetic");
            for (auto coefficient : coefficients) {
                if (coefficient) coefficient->setEnabled(arithmetic);
            }
        };
        if (operatorEditor) {
            connect(operatorEditor, qOverload<int>(&QComboBox::currentIndexChanged), this,
                    [updateCoefficientSensitivity](int) { updateCoefficientSensitivity(); });
            updateCoefficientSensitivity();
        }
    }
}

void PrimitiveSettingsWidget::setAttribute(const QString& attribute, const QString& value) {
    if (_loading.pending() || _writing.pending() || !_primitive || !_primitive->getRepr()) return;

    auto scoped = _writing.block();
    const auto key = attribute.toUtf8();
    const auto data = value.toUtf8();
    _primitive->setAttributeOrRemoveIfEmpty(key.constData(), data.constData());
    Q_EMIT primitiveChanged(_primitive, attribute, value);
}

} // namespace Linea::UI
