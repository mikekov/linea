// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * ElidingLabel — QLabel that auto-elides or fades out overflowing text.
 */

#ifndef LINEA_UI_ELIDING_LABEL_H
#define LINEA_UI_ELIDING_LABEL_H

#include <QLabel>
#include <QString>

class QResizeEvent;

namespace Linea::UI {

class ElidingLabel : public QLabel {
    Q_OBJECT

public:
    enum class OverflowMode {
        Elide, ///< Replace overflow with "…" (default)
        Fade,  ///< Fade out overflow with a horizontal alpha gradient
    };

    explicit ElidingLabel(QWidget* parent = nullptr);

    void setOverflowMode(OverflowMode mode);
    void setFullText(const QString& text);
    const QString& fullText() const;

protected:
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void updateElidedText();

    QString _fullText;
    OverflowMode _mode = OverflowMode::Fade;
};

} // namespace Linea::UI

#endif // LINEA_UI_ELIDING_LABEL_H
