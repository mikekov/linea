// SPDX-License-Identifier: GPL-2.0-or-later
/** @file Qt editor for SVG filter light source children. */

#ifndef LINEA_UI_FILTER_LIGHT_SOURCE_SETTINGS_WIDGET_H
#define LINEA_UI_FILTER_LIGHT_SOURCE_SETTINGS_WIDGET_H

#include <QString>
#include <QWidget>

#include "ui/operation-blocker.h"

class QComboBox;
class QGridLayout;
class SPFilterPrimitive;

namespace Linea::UI {

/** Editor for the feDistantLight, fePointLight, and feSpotLight child nodes. */
class LightSourceSettingsWidget final : public QWidget {
    Q_OBJECT

public:
    explicit LightSourceSettingsWidget(QWidget* parent = nullptr);

    SPFilterPrimitive* primitive() const { return _primitive; }
    void setPrimitive(SPFilterPrimitive* primitive);

Q_SIGNALS:
    void primitiveChanged(SPFilterPrimitive* primitive, const QString& attribute, const QString& value);

private:
    void clearForm();
    void load();
    void writeAttribute(const char* attribute, double value);
    void changeSource(int index);

    SPFilterPrimitive* _primitive = nullptr;
    QGridLayout* _form = nullptr;
    QComboBox* _source = nullptr;
    OperationBlocker _loading;
    OperationBlocker _writing;
};

} // namespace Linea::UI

#endif // LINEA_UI_FILTER_LIGHT_SOURCE_SETTINGS_WIDGET_H
