// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Expander — collapsible section header button.
 *
 * A button row showing an arrow and a label. The arrow points right
 * when collapsed and down when expanded. Clicking toggles the state.
 */

#ifndef LINEA_UI_EXPANDER_H
#define LINEA_UI_EXPANDER_H

#include <QString>
#include <QWidget>

class QToolButton;

namespace Linea::UI {

class Expander : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QString label READ label WRITE setLabel)
    Q_PROPERTY(State state READ state WRITE setState NOTIFY stateChanged)

public:
    enum State { Collapsed, Expanded };
    Q_ENUM(State)

    explicit Expander(QWidget* parent = nullptr);
    explicit Expander(const QString& label, QWidget* parent = nullptr);
    ~Expander() override = default;

    QString label() const;
    void setLabel(const QString& label);

    State state() const { return _state; }
    void setState(State state);

    bool isExpanded() const { return _state == Expanded; }
    void setExpanded(bool expanded);

Q_SIGNALS:
    void clicked();
    void stateChanged(Linea::UI::Expander::State state);

private:
    void updateIcon();

    void changeEvent(QEvent* event) override;

    QToolButton* _button;
    State _state = Collapsed;
};

} // namespace Linea::UI

#endif // LINEA_UI_EXPANDER_H
