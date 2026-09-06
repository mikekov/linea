// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Qt editor for SVG filter primitive attributes.
 */

#ifndef LINEA_UI_FILTER_PRIMITIVE_SETTINGS_WIDGET_H
#define LINEA_UI_FILTER_PRIMITIVE_SETTINGS_WIDGET_H

#include <QGridLayout>
#include <QWidget>
#include <QString>

#include "ui/operation-blocker.h"

class SPDesktop;
class SPFilterPrimitive;

namespace Linea::UI {

/**
 * Dynamically generated editor for the attributes of one SVG filter primitive.
 *
 * The widget edits the primitive's XML representation directly using the
 * explicitly supported primitive attribute specifications.
 */
class PrimitiveSettingsWidget final : public QWidget {
    Q_OBJECT

public:
    explicit PrimitiveSettingsWidget(QWidget* parent = nullptr);

    SPFilterPrimitive* primitive() const { return _primitive; }
    void setPrimitive(SPFilterPrimitive* primitive);
    void setDesktop(SPDesktop* desktop);

Q_SIGNALS:
    void primitiveChanged(SPFilterPrimitive* primitive, const QString& attribute, const QString& value);

private:
    void clearForm();
    void addSettings();
    void setAttribute(const QString& attribute, const QString& value);

    SPDesktop* _desktop = nullptr;
    SPFilterPrimitive* _primitive = nullptr;
    QGridLayout* _form = nullptr;
    OperationBlocker _loading;
    OperationBlocker _writing;
};

} // namespace Linea::UI

#endif // LINEA_UI_FILTER_PRIMITIVE_SETTINGS_WIDGET_H
