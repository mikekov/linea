// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * LinkedNumberPair widget - two NumberEdit fields with a link toggle button.
 *
 * Composite widget for paired numeric entries (X/Y, width/height, etc.).
 * Numeric properties are shared by both edits; per-side properties are
 * prefixed with first/second. The inner edits stay accessible via first()
 * and second() for signals and anything not exposed as a property.
 */

#ifndef LINEA_UI_LINKED_NUMBER_PAIR_H
#define LINEA_UI_LINKED_NUMBER_PAIR_H

#include <QWidget>
#include <memory>

class QToolButton;

QT_BEGIN_NAMESPACE
namespace Ui {
class LinkedNumberPair;
}
QT_END_NAMESPACE

namespace Linea::UI {

class NumberEdit;

class LinkedNumberPair : public QWidget {
    Q_OBJECT
    // Shared NumberEdit properties, applied to both edits
    Q_PROPERTY(double minimum READ minimum WRITE setMinimum)
    Q_PROPERTY(double maximum READ maximum WRITE setMaximum)
    Q_PROPERTY(int decimals READ decimals WRITE setDecimals)
    Q_PROPERTY(double singleStep READ singleStep WRITE setSingleStep)
    Q_PROPERTY(QString prefix READ prefix WRITE setPrefix)
    Q_PROPERTY(QString suffix READ suffix WRITE setSuffix)
    Q_PROPERTY(int keptFractionDigits READ keptFractionDigits WRITE setKeptFractionDigits)
    // Per-side properties
    Q_PROPERTY(QString firstIcon READ firstIcon WRITE setFirstIcon)
    Q_PROPERTY(QString secondIcon READ secondIcon WRITE setSecondIcon)
    Q_PROPERTY(QString firstLabel READ firstLabel WRITE setFirstLabel)
    Q_PROPERTY(QString secondLabel READ secondLabel WRITE setSecondLabel)
    Q_PROPERTY(QString firstToolTip READ firstToolTip WRITE setFirstToolTip)
    Q_PROPERTY(QString secondToolTip READ secondToolTip WRITE setSecondToolTip)
    Q_PROPERTY(double firstValue READ firstValue WRITE setFirstValue)
    Q_PROPERTY(double secondValue READ secondValue WRITE setSecondValue)
    // Link button
    Q_PROPERTY(bool linked READ isLinked WRITE setLinked NOTIFY linkToggled)
    Q_PROPERTY(QString linkToolTip READ linkToolTip WRITE setLinkToolTip)
    // When true and linked, edits mirror each other's value verbatim
    Q_PROPERTY(bool copyLinked READ copyLinked WRITE setCopyLinked)
    // Horizontal spacing between the three items (matches grid spacing)
    Q_PROPERTY(int spacing READ spacing WRITE setSpacing)

public:
    explicit LinkedNumberPair(QWidget* parent = nullptr);
    ~LinkedNumberPair() override;

    NumberEdit* first() const;
    NumberEdit* second() const;
    QToolButton* linkButton() const;

    bool isLinked() const;
    void setLinked(bool linked);

    bool copyLinked() const { return _copyLinked; }
    void setCopyLinked(bool copy) { _copyLinked = copy; }

    int spacing() const;
    void setSpacing(int spacing);

    // Shared numeric properties
    double minimum() const;
    void setMinimum(double minimum);
    double maximum() const;
    void setMaximum(double maximum);
    int decimals() const;
    void setDecimals(int decimals);
    double singleStep() const;
    void setSingleStep(double step);
    QString prefix() const;
    void setPrefix(const QString& prefix);
    QString suffix() const;
    void setSuffix(const QString& suffix);
    int keptFractionDigits() const;
    void setKeptFractionDigits(int digits);

    // Per-side properties
    QString firstIcon() const;
    void setFirstIcon(const QString& icon);
    QString secondIcon() const;
    void setSecondIcon(const QString& icon);
    QString firstLabel() const;
    void setFirstLabel(const QString& label);
    QString secondLabel() const;
    void setSecondLabel(const QString& label);
    QString firstToolTip() const;
    void setFirstToolTip(const QString& tip);
    QString secondToolTip() const;
    void setSecondToolTip(const QString& tip);
    double firstValue() const;
    void setFirstValue(double value);
    double secondValue() const;
    void setSecondValue(double value);
    QString linkToolTip() const;
    void setLinkToolTip(const QString& tip);

Q_SIGNALS:
    void linkToggled(bool linked);
    void firstChanged(double value);
    void secondChanged(double value);

private:
    std::unique_ptr<Ui::LinkedNumberPair> _ui;
    bool _copyLinked = false;
};

} // namespace Linea::UI

#endif // LINEA_UI_LINKED_NUMBER_PAIR_H
