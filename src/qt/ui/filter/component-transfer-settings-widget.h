// SPDX-License-Identifier: GPL-2.0-or-later
/** @file Qt editor for one feComponentTransfer primitive. */

#ifndef LINEA_UI_FILTER_COMPONENT_TRANSFER_SETTINGS_WIDGET_H
#define LINEA_UI_FILTER_COMPONENT_TRANSFER_SETTINGS_WIDGET_H

#include <QWidget>

#include "ui/operation-blocker.h"

class QComboBox;
class QGridLayout;
class QLineEdit;
class SPFilterPrimitive;

namespace Linea::UI {
class SpinScale;

/** Edits the four feFunc children of an feComponentTransfer primitive. */
class ComponentTransferSettingsWidget final : public QWidget {
    Q_OBJECT

public:
    explicit ComponentTransferSettingsWidget(QWidget* parent = nullptr);

    SPFilterPrimitive* primitive() const { return _primitive; }
    void setPrimitive(SPFilterPrimitive* primitive);

    int channel() const { return _channel; }
    void setChannel(int channel);

Q_SIGNALS:
    void primitiveChanged(SPFilterPrimitive* primitive, const QString& attribute, const QString& value);

private:
    void load();
    void ensureFuncNode();
    void writeAttribute(const char* attribute, const QString& value);
    void updateSensitivity();

    SPFilterPrimitive* _primitive = nullptr;
    QGridLayout* _form = nullptr;
    QComboBox* _channelCombo = nullptr;
    QComboBox* _typeCombo = nullptr;
    SpinScale* _slope = nullptr;
    SpinScale* _intercept = nullptr;
    SpinScale* _amplitude = nullptr;
    SpinScale* _exponent = nullptr;
    SpinScale* _offset = nullptr;
    QLineEdit* _tableValues = nullptr;
    int _channel = 0;
    OperationBlocker _loading;
    OperationBlocker _writing;
};

} // namespace Linea::UI

#endif // LINEA_UI_FILTER_COMPONENT_TRANSFER_SETTINGS_WIDGET_H
